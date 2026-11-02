# 008 — Android touch controls: a virtual SDL gamepad

Date: 2026-11-02. Status: accepted.

## Context
The Android build (ADR 007) needed a physical controller. The goal is to play on the phone alone: a stick for each thumb,
a few buttons for menus, and the overlay out of the way while a controller is connected.

## Options
1. **Patch recompinput** to read touch state next to the controllers, as the Goemon port does in its own input code. That
   means another RecompFrontend patch to carry, and each input path (game, menus, bindings) has to know about touch.
2. **A virtual SDL gamepad.** `SDL_JoystickAttachVirtualEx` with `SDL_JOYSTICK_TYPE_GAMECONTROLLER` (on in our SDL build,
   `SDL_JOYSTICK_VIRTUAL`). It supplies its own mapping and raises `SDL_CONTROLLERDEVICEADDED`, which `recompinput` handles
   like any pad (`input_events.cpp`).

## Decision
Option 2, with a small Java overlay (`TouchOverlay.java`, drawn with `Canvas`) and `src/main/android_touch.cpp`. Java only
stores state in atomics; the SDL calls happen once per frame in `update_gfx`, on the SDL event thread. The pad is detached
while a real controller is connected, so "One per player" mode doesn't see a phantom player. A real controller is a
non-virtual `SOURCE_JOYSTICK` device with an `AXIS_X` range, since phones have button devices that also claim to be gamepads.
Own code; nothing copied from the Goemon overlay.

## Consequences
- No RecompFrontend patch and no desktop change, other than the `__ANDROID__` call in `update_gfx`.
- Input is sent once per `update_gfx` call, not on each touch event. It felt right in the user's test.
- No layout editor, sensitivity setting or vibration yet. Button positions are fixed around the one-player HUD.
- Tested on the S26 Ultra: moving and aiming, pause menu, switching to and from the Xbox controller, Home and back.
