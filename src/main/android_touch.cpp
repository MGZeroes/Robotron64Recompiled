// On-screen controls on Android (ADR 008). android/.../TouchOverlay.java draws them and calls the JNI functions below
// on the UI thread; update() turns that state into a virtual SDL gamepad on the SDL event thread. recompinput then
// treats it like any other controller, for the game and for the menus.

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <optional>

#include <jni.h>

#include "SDL.h"

#include "recompui/recompui.h"
#include "robotron64_touch.h"

namespace {
    // TouchOverlay.java uses these numbers too, so an SDL update must not change them silently.
    static_assert(SDL_CONTROLLER_BUTTON_A == 0 && SDL_CONTROLLER_BUTTON_X == 2 && SDL_CONTROLLER_BUTTON_BACK == 4);
    static_assert(SDL_CONTROLLER_BUTTON_START == 6 && SDL_CONTROLLER_BUTTON_LEFTSHOULDER == 9);
    static_assert(SDL_CONTROLLER_AXIS_LEFTX == 0 && SDL_CONTROLLER_AXIS_LEFTY == 1);
    static_assert(SDL_CONTROLLER_AXIS_RIGHTX == 2 && SDL_CONTROLLER_AXIS_RIGHTY == 3);

    constexpr int stick_axes = 4;

    std::atomic<uint32_t> touch_buttons{0};
    std::array<std::atomic<float>, stick_axes> touch_axes{};
    std::atomic<bool> pad_wanted{false};

    SDL_Joystick* pad = nullptr;

    // The launcher and settings menus work by tapping, so the overlay hides while one is open.
    void report_menu_state() {
        static std::optional<bool> reported;
        bool menu_open = recompui::is_context_capturing_input();
        if (reported == menu_open) {
            return;
        }
        reported = menu_open;

        JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
        jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
        jclass activity_class = env->GetObjectClass(activity);
        env->CallVoidMethod(activity, env->GetMethodID(activity_class, "setMenuOpen", "(Z)V"), static_cast<jboolean>(menu_open));
        env->DeleteLocalRef(activity_class);
        env->DeleteLocalRef(activity);
    }

    void attach_pad() {
        SDL_VirtualJoystickDesc desc{};
        desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
        desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
        desc.naxes = SDL_CONTROLLER_AXIS_MAX;
        desc.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
        desc.name = "Touch controls";

        int device_index = SDL_JoystickAttachVirtualEx(&desc);
        if (device_index < 0) {
            fprintf(stderr, "Failed to attach the touch gamepad: %s\n", SDL_GetError());
            return;
        }
        pad = SDL_JoystickOpen(device_index);
        if (pad == nullptr) {
            fprintf(stderr, "Failed to open the touch gamepad: %s\n", SDL_GetError());
            SDL_JoystickDetachVirtual(device_index);
        }
    }

    void detach_pad() {
        SDL_JoystickClose(pad);
        pad = nullptr;
        // Device indexes shift as controllers come and go, so look it up.
        for (int i = 0; i < SDL_NumJoysticks(); i++) {
            if (SDL_JoystickIsVirtual(i)) {
                SDL_JoystickDetachVirtual(i);
                break;
            }
        }
    }
}

void robotron64::touch::update() {
    report_menu_state();

    bool wanted = pad_wanted.load(std::memory_order_relaxed);
    if (wanted && pad == nullptr) {
        attach_pad();
    }
    else if (!wanted && pad != nullptr) {
        detach_pad();
    }
    if (pad == nullptr) {
        return;
    }

    uint32_t buttons = touch_buttons.load(std::memory_order_relaxed);
    for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; button++) {
        SDL_JoystickSetVirtualButton(pad, button, (buttons >> button) & 1);
    }
    for (int axis = 0; axis < stick_axes; axis++) {
        float value = touch_axes[axis].load(std::memory_order_relaxed);
        SDL_JoystickSetVirtualAxis(pad, axis, static_cast<Sint16>(value * SDL_JOYSTICK_AXIS_MAX));
    }
}

extern "C" {
    JNIEXPORT void JNICALL Java_io_github_mgzeroes_robotron64_TouchOverlay_nativeSetState(JNIEnv*, jclass, jint buttons,
            jfloat left_x, jfloat left_y, jfloat right_x, jfloat right_y) {
        touch_buttons.store(static_cast<uint32_t>(buttons), std::memory_order_relaxed);
        touch_axes[SDL_CONTROLLER_AXIS_LEFTX].store(left_x, std::memory_order_relaxed);
        touch_axes[SDL_CONTROLLER_AXIS_LEFTY].store(left_y, std::memory_order_relaxed);
        touch_axes[SDL_CONTROLLER_AXIS_RIGHTX].store(right_x, std::memory_order_relaxed);
        touch_axes[SDL_CONTROLLER_AXIS_RIGHTY].store(right_y, std::memory_order_relaxed);
    }

    JNIEXPORT void JNICALL Java_io_github_mgzeroes_robotron64_TouchOverlay_nativeSetEnabled(JNIEnv*, jclass, jboolean enabled) {
        pad_wanted.store(enabled, std::memory_order_relaxed);
    }
}
