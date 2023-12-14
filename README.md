# GyroPalm FlexPoint
Control LVGL interfaces using Euclidean Ray-Casting and advanced techniques

## Overview

As part of GyroPalm's patented technology, GyroPalm provides developers with many different hands-free mechanisms for unparalleled gesture control. Elements on the screen of GyroPalm Encore such as LVGL buttons, checkboxes, and sliders can be manipulated with gestures, enabling any user to interact without specific gesture learning nor custom gesture programming.

A GyroPalm **FlexPoint gesture is a form of navigation gesture**, which can performed after a successful activation gesture. FlexPoint gestures enrich any `GyroPalmLVGL` interface since such gestures are easy to perform and require little to no hand-eye coordination. Unlike a traditional air mouse which requires users to precisely point to select an element, such as a button, a user can perform a FlexPoint selection with higher accuracy and less cognitive load. It is indeed one of the many specialties of GyroPalm's intuitive gesture methodology. **FlexPoint gestures can be performed quickly**, even by inexperienced users.

A **FlexPoint gesture involves a user subtly tilting their hand towards the general direction** of a desired LVGL widget. This interface involves an "elastic line" that is drawn on the screen. The user tilts their wrist to pivot the line to approach or "cross out" the desired widget before performing a snap gesture to select it. By snapping over a button, a click even is performed on said button. By snapping over a variable element such as a slider, an adjustment gesture interface is opened to allow further gesture manipulation of the control. While FlexPoint is available to all GyroPalm developers, not all GyroPalm apps may use FlexPoint. Some developers may find more meaningful integration with [GyroPalm's Customizable Gestures](https://app.gyropalm.com/studio/docs/firmware/#customizable-gestures) capability.

In the event that the user wants to select a button that is obstructed by another button in proximity, FlexPoint ensures that the further button is selected, as the user desires. This level of intelligent proportional control is made possible by the underlying algorithm observing all the widgets within the line of intersection and selecting the last widget under the final approach of the "elastic line".

## FlexPoint Technique

FlexPoint applies our advanced edge-based techniques to process IMU sensor data from the wearable, including a combination of Euclidean Ray-Casting, Temporal Smoothing, and Pre-Disruption Locking. When FlexPoint is activated, the boundaries of all LVGL elements are acquired and the centroids are determined. The IMU data is normalized and stabilized to map the dimensions of the screen. The Euclidean distance between the line of approach and one or more LVGL buttons are calculated and placed into an array for selection. Aside from constant real-time temporal smoothing, the wearable awaits an `onRawSnap` gesture from the user. Once the user performs a finger snap, a custom algorithm is performed to find the nearest LVGL element with a slight bias on the the data about 125 milliseconds before the snap. This is done as part of the Pre-Disruption locking, which ensures that the user's selection is not affected by the snap itself or other noise after selection. A user does not have to "perfectly" align the line of inference across their desired widget as the algorithm is graceful to human error by a few degrees offset. The results of using this technique allow for 99% gesture accuracy when gesture tests of 150 gesture interactions were performed on a variety of on-screen interfaces.

## FlexPoint Dependencies

To implement this SDK for its intended use, it is highly recommended that the developer has a GyroPalm Developer Kit or equivalent GyroPalm package to do proper testing. To order one or more wearables, visit the [GyroPalm Online store](https://gyropalm.com/order/).

FlexPoint requires the use of `GyroPalmEngine` and `GyroPalmLVGL`. If you do not have those implemented in your project, you may create a new project code using the `GyroPalm UI Designer` which will generate the bare minimum code you need to proceed. Ensure that you have already understood how to implement gesture callbacks. If you have not written gesture callbacks in GyroPalm before, please refer to the [Gesture Callbacks section](https://app.gyropalm.com/studio/docs/firmware/#gesture-callbacks). You will also need to first implement an activation gesture, prior to adding FlexPoint to your project. Read more about [GyroPalm Activation Gestures](https://app.gyropalm.com/studio/docs/gestures/#gyropalm-activation-gestures) for details.

## Installation and Usage

Please refer to GyroPalm's Studio Docs for further details on usage.