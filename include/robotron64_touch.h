#ifndef __ROBOTRON64_TOUCH_H__
#define __ROBOTRON64_TOUCH_H__

// On-screen controls on Android (ADR 008): TouchOverlay.java feeds a virtual SDL gamepad.
namespace robotron64::touch {
    // Call on the thread that pumps SDL events: attaches or detaches the pad and pushes the latest state to it.
    void update();
}

#endif
