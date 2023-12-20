/*************************************************** 
GyroPalm Encore - FlexPoint Dynamic Interface
Written by Dominick Lee for GyroPalm LLC.
Last Revised 12/19/2023.

This library is offered internally as a part of GyroPalm's developer kit.
Unless you have written permission from GyroPalm, the code is NOT intended for commercial use.

This library contains intellectual property belonging to GyroPalm.
MIT license only valid for GyroPalm LLC, explicitly authorized users, and internal products.

REQUIREMENTS:
- Your code must instantiate the GyroPalmEngine and GyroPalmLVGL objects
- Your code must have onRawSnap, onGlance, and onActivation callbacks
- FlexPoint only works when GyroPalm is activated first

USAGE:
1. Create a new project using GyroPalm UI Designer first. You'll have the bare minimum code.
2. Upload this file as part of your project. Then put in main code: #include "GyroPalmFlexPoint.h"
3. Add this line before the end of void setup(): flexPointSetup(&gplm);
4. Add this line at the beginning of void loop(): flexPointLoop();
5. Make sure you have the following declared in your void setup() below listenEvents():
    gplm.autoTimeout = true;    //tells the wearable to deactivate automatically
    gplm.deactivateTimeout = 4000;  // (optional) timeout in miliseconds (3 seconds default)
    gplm.activationGesture = ACT_DOUBLE_SNAP;   // (optional) ACT_DOUBLE_SNAP by default
    gplm.setActivationCallback(onActivation);   // register activation gesture callback

    gplm.setRawSnapCallback(onRawSnap);
    gplm.setGlanceCallback(onGlance);
    gplm.setPwrQuickPressCallback(onPwrQuickPress);
    delay(100);
6. Write the onRawSnap callback:
    void onRawSnap()
    {
        flexPointSnap();
    }
7. Write the onActivation callback:
    void onActivation(bool isActive)
    {
        if (isActive) {
            Serial.println("Activated!");
            form[curScreen].setIconColor(BAR_GLANCE, LV_COLOR_CYAN);
            // your code here, once wearable is activated
            lv_disp_trig_activity(NULL);    //trigger user activity
        } else {
            Serial.println("Deactivated!");
            form[curScreen].setIconColor(BAR_GLANCE, LV_COLOR_WHITE);
            form[curScreen].hideIcon(BAR_GLANCE);
            // your code here, once wearable is deactivated
        }

        flexPointShow(isActive);
    }
8. On every "case" in void showApp(int page), add this before the form[curScreen].showScreen function:
    flexPointInterface(&form[curScreen]);
9. (optional) Call flexPointRapid() in your onShake() gesture callback if you want to keep FlexPoint active onShake:
    gplm.setShakeCallback(onShake);
    gplm.setMaxShakes(5); 
    void onShake(int numShakes)
    {
        if (numShakes >= 5) {
            flexPointRapid();
        }
    }
You would treat the above like adding an LVGL widget.
****************************************************/

GyroPalmEngine* _fpGPLM;
GyroPalmLVGL * _fpScreen;
lv_obj_t * fpLine1;
lv_task_t *taskFuzzyGest;
void lv_fuzzyGest_task(struct _lv_task_t *);

const int fpScreenWidth = 240;
const int fpScreenHeight = 240;
const int xOriginOffset = 0;  // Origin of line start offset
const int yOriginOffset = 10;  // Origin of line start offset
lv_obj_t *lastSelWidget; // Last selected widget
bool enableFuzzySelector = false;
bool fpLineHidden = true;
bool fpAdjustMode = false;
long fpAdjustStarted = 0;
bool fpRapidEnabled = false;

// For rolling window
typedef struct {
    int ax;
    int ay;
    unsigned long timestamp; // Store the time when the reading was taken
} AccReading;
#define BUFFER_SIZE 15 // Size of the circular buffer
AccReading accBuffer[BUFFER_SIZE];
int fpBufferIndex = 0;
bool flexPointVibrate = false;

// For rolling window
void updateAccBuffer(int ax, int ay) {
    accBuffer[fpBufferIndex].ax = ax;
    accBuffer[fpBufferIndex].ay = ay;
    accBuffer[fpBufferIndex].timestamp = millis();

    fpBufferIndex = (fpBufferIndex + 1) % BUFFER_SIZE; // Move to the next position
}

AccReading getOldAccReading(unsigned long targetTime) {
    AccReading closestReading;
    unsigned long closestTimeDiff = ULONG_MAX;

    for (int i = 0; i < BUFFER_SIZE; i++) {
        unsigned long timeDiff = targetTime > accBuffer[i].timestamp ? 
                                 targetTime - accBuffer[i].timestamp : 
                                 accBuffer[i].timestamp - targetTime;
        
        if (timeDiff < closestTimeDiff) {
            closestTimeDiff = timeDiff;
            closestReading = accBuffer[i];
        }
    }

    return closestReading;
}

void updateLineFP() {
    // Assuming fpScreenWidth and fpScreenHeight are the dimensions of your screen

    updateAccBuffer(_fpGPLM->ax, _fpGPLM->ay);  // populate rolling window for later

    // Scale factors - adjust based on how sensitive you want the mapping to be
    float scaleX = (float)fpScreenWidth / 720;  // Assuming full tilt covers the entire width
    float scaleY = (float)fpScreenHeight / 720; // Assuming full tilt covers the entire height

    // Calculate new end point based on accelerometer data
    int endX = (fpScreenWidth / 2) + (int)(_fpGPLM->ax * scaleX);
    int endY = (fpScreenHeight / 2) + (int)(_fpGPLM->ay * scaleY);

    // Ensure the end point is within screen bounds
    endX = (endX < 0) ? 0 : (endX > fpScreenWidth) ? fpScreenWidth : endX;
    endY = (endY < 0) ? 0 : (endY > fpScreenHeight) ? fpScreenHeight : endY;

    // Update the line points
    static lv_point_t line_points[2];
    line_points[0] = (lv_point_t){(fpScreenWidth / 2) + xOriginOffset, (fpScreenHeight / 2) + yOriginOffset}; // Start point (center)
    line_points[1] = (lv_point_t){endX, endY};                        // End point (calculated)

    // Update the line object
    lv_line_set_points(fpLine1, line_points, 2);
}

bool isCompatibleWidget(lv_obj_t* widget) {
    lv_obj_type_t obj_type;
    lv_obj_get_type(widget, &obj_type);

    // Check if the object type matches any of the compatible widgets
    if (strcmp(obj_type.type[0], "lv_btn") == 0 || 
        strcmp(obj_type.type[0], "lv_checkbox") == 0 ||
        strcmp(obj_type.type[0], "lv_imgbtn") == 0 ||
        strcmp(obj_type.type[0], "lv_switch") == 0 ||
        strcmp(obj_type.type[0], "lv_slider") == 0) {
        return true;
    }
    return false;
}

bool isWidgetCheckBox(lv_obj_t* widget) {
    lv_obj_type_t obj_type;
    lv_obj_get_type(widget, &obj_type);

    // Check if the object type matches any of the compatible widgets
    if (strcmp(obj_type.type[0], "lv_checkbox") == 0) {
        return true;
    }
    return false;
}

bool isWidgetSlider(lv_obj_t* widget) {
    lv_obj_type_t obj_type;
    lv_obj_get_type(widget, &obj_type);

    // Check if the object type matches any of the compatible widgets
    if (strcmp(obj_type.type[0], "lv_slider") == 0) {
        return true;
    }
    return false;
}

void selectWidget() {
    float angleTolerance = 10 * (M_PI / 180); // Convert 10 degrees to radians

    // Calculate the angle of the line of approach
    float angle = atan2(_fpGPLM->ay, _fpGPLM->ax);

    // Calculate the length of the line of approach based on tilt amplitude
    float amplitude = sqrt(_fpGPLM->ax * _fpGPLM->ax + _fpGPLM->ay * _fpGPLM->ay);
    float lineLength = (amplitude / 360) * (sqrt(fpScreenWidth * fpScreenWidth + fpScreenHeight * fpScreenHeight) / 2);

    // Calculate the endpoint of the line
    float endX = (fpScreenWidth / 2) + xOriginOffset + (lineLength * cos(angle));
    float endY = (fpScreenHeight / 2) + yOriginOffset + (lineLength * sin(angle));

    lv_obj_t* closestWidget = NULL;
    float closestDistanceToEnd = 32767;
    lv_obj_t* child;
    lv_obj_t* parent = _fpScreen->_screen;

    // Iterate through all child objects (widgets) on the screen
    child = lv_obj_get_child(parent, NULL);
    while (child) {

        // Check if the object is compatible
        if (isCompatibleWidget(child)) {  // Only select buttons
            lv_area_t widget_area;
            lv_obj_get_coords(child, &widget_area);
            float widget_center_x = (widget_area.x1 + widget_area.x2) / 2;
            float widget_center_y = (widget_area.y1 + widget_area.y2) / 2;

            // Calculate the distance from the widget to the line's endpoint
            float distanceToEnd = sqrt(pow(widget_center_x - endX, 2) + pow(widget_center_y - endY, 2));

            float widget_angle = atan2(widget_center_y - ((fpScreenHeight / 2) + yOriginOffset), 
                                    widget_center_x - ((fpScreenWidth / 2) + xOriginOffset));

            if (fabs(widget_angle - angle) <= angleTolerance && distanceToEnd <= lineLength) {
                if (distanceToEnd < closestDistanceToEnd) {
                    closestDistanceToEnd = distanceToEnd;
                    closestWidget = child;
                }
            }
        }

        // Move to the next child object
        child = lv_obj_get_child(parent, child);
    }

     // Clear focus state from all widgets
    child = lv_obj_get_child(parent, NULL);
    while (child) {

        if (isCompatibleWidget(child)) {
            lv_obj_clear_state(child, LV_STATE_FOCUSED);
        }
        child = lv_obj_get_child(parent, child);
    }   

    // If a widget is found, give focus to it
    if (closestWidget != NULL) {
        lv_obj_add_state(closestWidget, LV_STATE_FOCUSED);
        
        if (closestWidget != lastSelWidget) {
            lastSelWidget = closestWidget;
            flexPointVibrate = true;
        }
    }
}

void startfpAdjustMode() {
    fpAdjustMode = true;
    fpAdjustStarted = millis();
    flexPointVibrate = true;
}

void lv_fuzzyGest_task(struct _lv_task_t *data) {
    if (enableFuzzySelector) {
        updateLineFP();
        selectWidget();
    } else {
        if (fpLineHidden != true) {
            lv_obj_set_hidden(fpLine1, true);
            fpLineHidden = true;
        }
    }
    //Handle fpAdjustMode
    if (fpAdjustMode && (millis() - fpAdjustStarted) > 5000) {  //Adjustmode timeout
        fpAdjustMode = false;
        flexPointVibrate = true;
    }

    if (fpAdjustMode) {
        if (isWidgetSlider(lastSelWidget)) {
            int setVal = map(_fpGPLM->ax, -300, 300, 0, 100);
            lv_slider_set_value(lastSelWidget, setVal, LV_ANIM_ON);
        }
    }
}

void flexPointInterface(GyroPalmLVGL *screen) {
    _fpScreen = screen;
    
    static lv_point_t line_points[] = {{240 / 2, 240 / 2}, {240 / 2, 0}};  // Start from center, end at top (as an example)
    
    // Create a style
    static lv_style_t style_line;
    lv_style_init(&style_line);
    lv_style_set_line_width(&style_line, LV_STATE_DEFAULT, 4);
    lv_style_set_line_color(&style_line, LV_STATE_DEFAULT, LV_COLOR_AQUA);
    lv_style_set_line_rounded(&style_line, LV_STATE_DEFAULT, true);

    // Create a line object
    fpLine1 = lv_line_create(_fpScreen->_screen, NULL);
    lv_line_set_points(fpLine1, line_points, 2);  // Set the points
    lv_obj_add_style(fpLine1, LV_LINE_PART_MAIN, &style_line); 
    lv_obj_set_hidden(fpLine1, true);
}

void flexPointSnap()
{
    if (_fpGPLM->isActive) {
        Serial.println("snapped");
        enableFuzzySelector = false;
        lv_task_handler();
        // Assuming you want to go back by 130 milliseconds
        unsigned long targetTime = millis() - 130;
        AccReading oldReading = getOldAccReading(targetTime);
        _fpGPLM->ax = oldReading.ax;
        _fpGPLM->ay = oldReading.ay;
        selectWidget();
        fpLineHidden = false;

        if (isWidgetCheckBox(lastSelWidget)) {
            // Manually toggle the checkbox state
            bool isChecked = lv_checkbox_is_checked(lastSelWidget);
            lv_checkbox_set_checked(lastSelWidget, !isChecked);

            // Manually call the checkbox's event callback
            lv_event_cb_t cb = lv_obj_get_event_cb(lastSelWidget);
            if (cb) {
                cb(lastSelWidget, LV_EVENT_VALUE_CHANGED);
            }
            
            if (fpRapidEnabled) {
                _fpGPLM->setActive(true);  // Reset timeout. Keep active for next selection
            } else {
                _fpGPLM->setActive(false);  // Action completed, redeem activation
            }
        } 
        else if (isWidgetSlider(lastSelWidget)) {
            if (fpAdjustMode == false) {
                startfpAdjustMode();    // Adjusting a slider using gesture
            }
        }
        else {
            // Send a clicked event for other widget types
            lv_event_send(lastSelWidget, LV_EVENT_CLICKED, NULL);
            
            if (fpRapidEnabled) {
                _fpGPLM->setActive(true);  // Reset timeout. Keep active for next selection
            } else {
                _fpGPLM->setActive(false);  // Action completed, redeem activation
            }
        }
        
        lv_disp_trig_activity(NULL);    //trigger user activity
    }

    if (fpAdjustMode && millis() - fpAdjustStarted > 200) { // if user snaps to disable adjust mode
        fpAdjustMode = false;
        flexPointVibrate = true;
        
        if (fpRapidEnabled) {
            _fpGPLM->setActive(true);  // Reset timeout. Keep active for next selection
        } else {
            _fpGPLM->setActive(false);  // Action completed, redeem activation
        }
    }
}

void flexPointSetup(GyroPalmEngine* engine)
{
    _fpGPLM = engine;
    taskFuzzyGest = lv_task_create(lv_fuzzyGest_task, 80, LV_TASK_PRIO_LOWEST, NULL);
}

void flexPointLoop()
{
    if (flexPointVibrate) {
        _fpGPLM->vibrateTap();
        flexPointVibrate = false;
    }
}

void flexPointShow(bool isEnabled) 
{
    enableFuzzySelector = isEnabled;
    lv_obj_set_hidden(fpLine1, !isEnabled);
    fpLineHidden = !isEnabled;
    
    if (isEnabled == false) {
        fpRapidEnabled = false;
    }
}

void flexPointRapid()
{
    if (_fpGPLM->isActive) {
        fpRapidEnabled = true;
        flexPointVibrate = true;
        _fpGPLM->setActive(true);  // Reset timeout. Keep active for next selection
    }
}