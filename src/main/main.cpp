#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cassert>
#include <vector>
#include <array>
#include <filesystem>
#include <stdexcept>
#include <cinttypes>
#include <chrono>
#include <memory>

#if !defined(__ANDROID__)
#include "nfd.h"
#endif

#include "ultramodern/ultra64.h"
#include "ultramodern/ultramodern.hpp"
#include "ultramodern/config.hpp"
#define SDL_MAIN_HANDLED
#ifdef _WIN32
#include "SDL.h"
#else
#include "SDL2/SDL.h"
#include "SDL2/SDL_syswm.h"
// Undefine x11 macros that get included by SDL_syswm.h.
#undef None
#undef Status
#undef LockMask
#undef ControlMask
#undef Success
#undef Always
#endif

#include "recompui/recompui.h"
#include "recompui/program_config.h"
#include "recompui/renderer.h"
#include "recompui/config.h"
#include "util/file.h"
#include "recompinput/input_events.h"
#include "recompinput/recompinput.h"
#include "recompinput/profiles.h"
#include "robotron64_config.h"
#include "robotron64_launcher.h"
#if defined(__ANDROID__)
#include "robotron64_files.h"
#include "robotron64_touch.h"
#endif
#include "recomp_data.h"
#include "ovl_patches.hpp"
#include "theme.h"
#include "librecomp/game.hpp"
#include "librecomp/mods.hpp"
#include "librecomp/helpers.hpp"

#include "../../patches/graphics.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <timeapi.h>
#include "SDL_syswm.h"
#endif

#include "../../lib/rt64/src/contrib/stb/stb_image.h"

const std::string version_string = ROBOTRON64_VERSION;

template<typename... Ts>
void exit_error(const char* str, Ts ...args) {
    ((void)fprintf(stderr, str, args), ...);
    assert(false);
        
    ultramodern::error_handling::quick_exit(__FILE__, __LINE__, __FUNCTION__);
}

ultramodern::gfx_callbacks_t::gfx_data_t create_gfx() {
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC) != 0) {
        exit_error("Failed to initialize SDL2: %s\n", SDL_GetError());
    }

    fprintf(stdout, "SDL Video Driver: %s\n", SDL_GetCurrentVideoDriver());

    return {};
}

// Controller ports. The game uses two N64 ports per player: player 1 moves with port 0 and aims with the
// analog stick of port 1, player 2 uses ports 2 and 3 (decomp src/game/player/behavior.c). It binds
// player 2 to port 2 only if that port is connected when the options load (session_input_initialize.c),
// so all four ports always report a controller and the bindings never change.
ultramodern::input::connected_device_info_t get_connected_device_info(int controller_num) {
    return ultramodern::input::connected_device_info_t{
        .connected_device = ultramodern::input::Device::Controller,
        .connected_pak = ultramodern::input::Pak::RumblePak,
    };
}

// The recompinput player whose pad feeds this port. Shared (single-player mode): 0, all pads merged.
// One per player: port / 2; a player without an assigned pad mirrors player 1, so one pad still plays both.
static int player_for_port(int controller_num) {
    if (recompinput::players::is_single_player_mode()) {
        return 0;
    }
    int player = controller_num >> 1;
    return recompinput::players::get_player_is_assigned(player) ? player : 0;
}

// Analog aiming: with the option on, the odd (aim) ports carry only the player's right stick. Held fire
// direction bits (D-pad / C buttons / keyboard) still take priority in the game, so those stay 8-way.
// First person with Modern controls (first_person.cpp, single player): player 1's pad walks with the left
// stick (the game reads port 0's stick) and turns with the right stick or C-left/right (handed to the
// movement hook). A or B fires ahead: the game makes shots relative to the facing in this view, so C-up is
// "ahead". The other C buttons and the aim stick (port 1) are muted so turning never fires.
static bool get_first_person_input(int controller_num, int player, uint16_t* buttons, float* x, float* y) {
    constexpr uint16_t button_a = 0x8000;
    constexpr uint16_t button_b = 0x4000;
    constexpr uint16_t c_up = 0x0008;
    constexpr uint16_t c_left = 0x0002;
    constexpr uint16_t c_right = 0x0001;
    constexpr uint16_t c_buttons = 0x000F;
    constexpr float analog_turn_threshold = 0.05f;

    if (controller_num == 1) {
        *buttons = 0;
        *x = 0.0f;
        *y = 0.0f;
        return true;
    }
    bool connected = recompinput::profiles::get_n64_input(player, buttons, x, y);
    float right_x = 0.0f;
    float right_y = 0.0f;
    if (!recompinput::game_input_disabled()) {
        recompinput::get_right_analog(player, &right_x, &right_y);
    }
    // With Analog Aiming off the right stick also produces C buttons; use its analog value when it moves.
    float turn = right_x;
    if (std::fabs(right_x) < analog_turn_threshold) {
        turn = ((*buttons & c_right) ? 1.0f : 0.0f) - ((*buttons & c_left) ? 1.0f : 0.0f);
    }
    robotron64::set_first_person_turn(std::clamp(turn, -1.0f, 1.0f));
    *buttons &= ~c_buttons;
    if (*buttons & (button_a | button_b)) {
        *buttons |= c_up;
    }
    return connected;
}

bool get_n64_input(int controller_num, uint16_t* buttons, float* x, float* y) {
    int player = player_for_port(controller_num);
    if (controller_num < 2 && robotron64::first_person_modern_active()) {
        return get_first_person_input(controller_num, player, buttons, x, y);
    }
    if ((controller_num & 1) && robotron64::analog_aim_enabled()) {
        float right_x = 0.0f;
        float right_y = 0.0f;
        if (!recompinput::game_input_disabled()) {
            recompinput::get_right_analog(player, &right_x, &right_y);
        }
        *buttons = 0;
        *x = right_x;
        *y = -right_y; // SDL's Y axis points down, the N64's up.
        return true;
    }
    return recompinput::profiles::get_n64_input(player, buttons, x, y);
}

// One per player: recompinput's assignment commit (players.cpp) sets only the assigned device's profile, and a
// controller profile left at index 0 is Keyboard (SP), so the keyboard would drive both players. After each
// assignment the keyboard drives only its assigned player (their "Keyboard (Player N)" profile, filled by
// config.cpp), or player 1 with Keyboard (SP) if unassigned; other keyboard profiles are -1 (skipped), and
// controller profiles that would point at a keyboard get the SP controller profile.
// Runs on the game thread and waits for two unchanged polls, so a commit on the UI thread is never half-applied.
// Profiles picked later on the player cards are kept until the next assignment.
static void update_player_profiles() {
    using namespace recompinput;
    static std::array<uintptr_t, 5> applied{};
    static std::array<uintptr_t, 5> pending{};
    static int stable_polls = 0;

    constexpr int num_players = 2;
    std::array<uintptr_t, 5> current{ players::get_number_of_assigned_players() };
    for (int i = 0; i < num_players; i++) {
        current[1 + i * 2] = static_cast<uintptr_t>(players::get_player_input_device(i));
        current[2 + i * 2] = reinterpret_cast<uintptr_t>(players::get_player(i).controller);
    }

    // Confirming the same layout again also rewrites the profiles, so every finished assignment counts as a
    // change. The commit runs after the assignment stops being active, hence the two stable polls below.
    static bool was_assigning = false;
    if (playerassignment::is_active()) {
        was_assigning = true;
        return;
    }
    if (was_assigning) {
        was_assigning = false;
        applied.fill(~uintptr_t{});
        pending = current;
        stable_polls = 0;
        return;
    }
    if (players::is_single_player_mode()) {
        return;
    }
    if (current != pending) {
        pending = current;
        stable_polls = 0;
        return;
    }
    if (current == applied || ++stable_polls < 2) {
        return;
    }
    applied = current;

    bool keyboard_taken = false;
    for (int i = 0; i < num_players; i++) {
        keyboard_taken |= players::get_player_input_device(i) == InputDevice::Keyboard;
    }
    int sp_keyboard = profiles::get_sp_keyboard_profile_index();
    int sp_controller = profiles::get_sp_controller_profile_index();
    for (int i = 0; i < num_players; i++) {
        InputDevice device = players::get_player_input_device(i);
        if (device != InputDevice::Controller) {
            profiles::set_input_profile_for_player(i, sp_controller, InputDevice::Controller);
        }
        if (device != InputDevice::Keyboard) {
            bool keeps_keyboard = i == 0 && !keyboard_taken;
            profiles::set_input_profile_for_player(i, keeps_keyboard ? sp_keyboard : -1, InputDevice::Keyboard);
        }
    }
}

void poll_inputs() {
    update_player_profiles();
    recompinput::poll_inputs();
}

// Rumble comes on the player's movement port (0 or 2). recompinput rumbles channel N on player N's pad
// (all pads in single-player mode, where only channel 0 counts).
void set_rumble(int controller_num, bool rumble) {
    if (controller_num & 1) {
        return;
    }
    recompinput::set_rumble(recompinput::players::is_single_player_mode() ? controller_num : controller_num >> 1, rumble);
}

// App icon. On Windows the exe's icon resource (icons/app.rc) is used for the window too: SDL registers
// its window class with the first RT_GROUP_ICON in the module (SDL 2.30.3 SDL_windowsevents.c,
// SDL_RegisterApp). Elsewhere the window icon is set from icons/app.png, embedded as icon_bytes.
#if !defined(_WIN32)
#include "icon_bytes.h"

static void set_window_icon(SDL_Window* window) {
    int width, height, channels;
    stbi_uc* data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(icon_bytes), static_cast<int>(icon_bytes_size),
        &width, &height, &channels, 4);
    if (data == nullptr) {
        return;
    }
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(data, width, height, 32, width * 4, SDL_PIXELFORMAT_RGBA32);
    if (surface != nullptr) {
        SDL_SetWindowIcon(window, surface);
        SDL_FreeSurface(surface);
    }
    stbi_image_free(data);
}
#endif

SDL_Window* window;

ultramodern::renderer::WindowHandle create_window(ultramodern::gfx_callbacks_t::gfx_data_t) {
    uint32_t flags = SDL_WINDOW_RESIZABLE;

#if defined(__APPLE__)
    flags |= SDL_WINDOW_METAL;
#elif defined(RT64_SDL_WINDOW_VULKAN)
    flags |= SDL_WINDOW_VULKAN;
#endif

#if defined(__ANDROID__)
    // A fullscreen window makes SDLActivity hide the system bars; a resizable one would also let it rotate to portrait.
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    flags = (flags & ~SDL_WINDOW_RESIZABLE) | SDL_WINDOW_FULLSCREEN;
#endif

    window = SDL_CreateWindow("Robotron 64: Recompiled", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1600, 900,  flags);

    if (window == nullptr) {
        exit_error("Failed to create window: %s\n", SDL_GetError());
    }

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);

#if !defined(_WIN32)
    set_window_icon(window);
#endif

#if defined(_WIN32)
    return ultramodern::renderer::WindowHandle{ wmInfo.info.win.window, GetCurrentThreadId() };
#elif defined(__linux__) || defined(__ANDROID__)
    return ultramodern::renderer::WindowHandle{ window };
#elif defined(__APPLE__)
    SDL_MetalView view = SDL_Metal_CreateView(window);
    return ultramodern::renderer::WindowHandle{ wmInfo.info.cocoa.window,  SDL_Metal_GetLayer(view) };
#else
    static_assert(false && "Unimplemented");
#endif
}

void update_gfx(void*) {
#if defined(__ANDROID__)
    robotron64::touch::update();
    robotron64::files::update();
#endif
    recompinput::handle_events();
}

static SDL_AudioCVT audio_convert;
static SDL_AudioDeviceID audio_device = 0;

// Samples per channel per second.
static uint32_t sample_rate = 48000;
static uint32_t output_sample_rate = 48000;
// Channel count.
constexpr uint32_t input_channels = 2;
static uint32_t output_channels = 2;

// Terminology: a frame is a collection of samples for each channel. e.g. 2 input samples is one input frame. This is unrelated to graphical frames.

// Number of frames to duplicate for fixing interpolation at the start and end of a chunk.
constexpr uint32_t duplicated_input_frames = 4;
// The number of output frames to skip for playback (to avoid playing duplicate inputs twice).
static uint32_t discarded_output_frames;

constexpr uint32_t bytes_per_frame = input_channels * sizeof(float);

void queue_samples(int16_t* audio_data, size_t sample_count) {
    // An N64 audio buffer is a few thousand samples; drop anything far larger instead of reading past it.
    // The osAiGetLength hook in robotron64.us.toml fixes the known cause.
    constexpr size_t max_sane_sample_count = 0x8000;
    if (sample_count > max_sane_sample_count) {
        static int bad_chunk_logs = 0;
        if (bad_chunk_logs++ < 20) {
            fprintf(stderr, "[robotron] dropped audio chunk: %zu samples\n", sample_count);
        }
        return;
    }

    // Buffer for holding the output of swapping the audio channels. This is reused across
    // calls to reduce runtime allocations.
    static std::vector<float> swap_buffer;
    static std::array<float, duplicated_input_frames * input_channels> duplicated_sample_buffer;

    // Make sure the swap buffer is large enough to hold the audio data, including any extra space needed for resampling.
    size_t resampled_sample_count = sample_count + duplicated_input_frames * input_channels;
    size_t max_sample_count = std::max(resampled_sample_count, resampled_sample_count * audio_convert.len_mult);
    if (max_sample_count > swap_buffer.size()) {
        swap_buffer.resize(max_sample_count);
    }
    
    // Copy the duplicated frames from last chunk into this chunk
    for (size_t i = 0; i < duplicated_input_frames * input_channels; i++) {
        swap_buffer[i] = duplicated_sample_buffer[i];
    }

    // Convert the audio from 16-bit values to floats and swap the audio channels into the
    // swap buffer to correct for the address xor caused by endianness handling.
    float cur_main_volume = static_cast<float>(recompui::config::sound::get_main_volume()) / 100.0f; // Get the current main volume, normalized to 0.0-1.0.
    for (size_t i = 0; i < sample_count; i += input_channels) {
        swap_buffer[i + 0 + duplicated_input_frames * input_channels] = audio_data[i + 1] * (0.5f / 32768.0f) * cur_main_volume;
        swap_buffer[i + 1 + duplicated_input_frames * input_channels] = audio_data[i + 0] * (0.5f / 32768.0f) * cur_main_volume;
    }
    
    // TODO handle cases where a chunk is smaller than the duplicated frame count.
    assert(sample_count > duplicated_input_frames * input_channels);

    // Copy the last converted samples into the duplicated sample buffer to reuse in resampling the next queued chunk.
    for (size_t i = 0; i < duplicated_input_frames * input_channels; i++) {
        duplicated_sample_buffer[i] = swap_buffer[i + sample_count];
    }
    
    audio_convert.buf = reinterpret_cast<Uint8*>(swap_buffer.data());
    audio_convert.len = (sample_count + duplicated_input_frames * input_channels) * sizeof(swap_buffer[0]);

    int ret = SDL_ConvertAudio(&audio_convert);

    if (ret < 0) {
        printf("Error using SDL audio converter: %s\n", SDL_GetError());
        throw std::runtime_error("Error using SDL audio converter");
    }

    uint64_t cur_queued_microseconds = uint64_t(SDL_GetQueuedAudioSize(audio_device)) / bytes_per_frame * 1000000 / sample_rate;
    uint32_t num_bytes_to_queue = audio_convert.len_cvt - output_channels * discarded_output_frames * sizeof(swap_buffer[0]);
    float* samples_to_queue = swap_buffer.data() + output_channels * discarded_output_frames / 2;

    // Prevent audio latency from building up by skipping samples in incoming audio when too many samples are already queued.
    // Skip samples based on how many microseconds of samples are queued already.
    uint32_t skip_factor = cur_queued_microseconds / 100000;
    if (skip_factor != 0) {
        uint32_t skip_ratio = 1 << skip_factor;
        num_bytes_to_queue /= skip_ratio;
        for (size_t i = 0; i < num_bytes_to_queue / (output_channels * sizeof(swap_buffer[0])); i++) {
            samples_to_queue[2 * i + 0] = samples_to_queue[2 * skip_ratio * i + 0];
            samples_to_queue[2 * i + 1] = samples_to_queue[2 * skip_ratio * i + 1];
        }
    }

    // Queue the swapped audio data.
    // Offset the data start by only half the discarded frame count as the other half of the discarded frames are at the end of the buffer.
    SDL_QueueAudio(audio_device, samples_to_queue, num_bytes_to_queue);
}

size_t get_frames_remaining() {
    constexpr float buffer_offset_frames = 1.0f;
    // Get the number of remaining buffered audio bytes.
    uint64_t buffered_byte_count = SDL_GetQueuedAudioSize(audio_device);

    // Scale the byte count based on the ratio of sample rates and channel counts.
    buffered_byte_count = buffered_byte_count * 2 * sample_rate / output_sample_rate / output_channels;

    // Adjust the reported count to be some number of refreshes in the future, which helps ensure that
    // there are enough samples even if the audio thread experiences a small amount of lag. This prevents
    // audio popping on games that use the buffered audio byte count to determine how many samples
    // to generate.
    uint32_t frames_per_vi = (sample_rate / 60);
    if (buffered_byte_count > (buffer_offset_frames * bytes_per_frame * frames_per_vi)) {
        buffered_byte_count -= (buffer_offset_frames * bytes_per_frame * frames_per_vi);
    }
    else {
        buffered_byte_count = 0;
    }
    // Convert from byte count to sample count.
    return static_cast<uint32_t>(buffered_byte_count / bytes_per_frame);
}

void update_audio_converter() {
    int ret = SDL_BuildAudioCVT(&audio_convert, AUDIO_F32, input_channels, sample_rate, AUDIO_F32, output_channels, output_sample_rate);

    if (ret < 0) {
        printf("Error creating SDL audio converter: %s\n", SDL_GetError());
        throw std::runtime_error("Error creating SDL audio converter");
    }

    // Calculate the number of samples to discard based on the sample rate ratio and the duplicate frame count.
    discarded_output_frames = duplicated_input_frames * output_sample_rate / sample_rate;
}

void set_frequency(uint32_t freq) {
    sample_rate = freq;
    
    update_audio_converter();
}

bool reset_audio(uint32_t output_freq) {
    SDL_AudioSpec spec_desired{
        .freq = (int)output_freq,
        .format = AUDIO_F32,
        .channels = (Uint8)output_channels,
        .silence = 0, // calculated
        .samples = 0x100, // Fairly small sample count to reduce the latency of internal buffering
        .padding = 0, // unused
        .size = 0, // calculated
        .callback = nullptr,
        .userdata = nullptr
    };

    audio_device = SDL_OpenAudioDevice(nullptr, false, &spec_desired, nullptr, 0);
    if (audio_device == 0) {
        std::string audio_error = std::string("No audio device could be found. Please make sure an audio device is available.\nError opening audio device: ") + std::string(SDL_GetError());
        recompui::message_box(audio_error.c_str());
        return false;
    }

    SDL_PauseAudioDevice(audio_device, 0);

    output_sample_rate = output_freq;
    update_audio_converter();

    return true;
}

extern RspUcodeFunc aspMain;

RspUcodeFunc* get_rsp_microcode(const OSTask* task) {
    switch (task->t.type) {
    case M_AUDTASK:
        return aspMain;

    default:
        fprintf(stderr, "Unknown task: %" PRIu32 "\n", task->t.type);
        return nullptr;
    }
}

extern "C" void recomp_entrypoint(uint8_t * rdram, recomp_context * ctx);
gpr get_entrypoint_address();

std::vector<recomp::GameEntry> supported_games = {
    {
        // XXH3-64 of Robotron 64 (USA) rev 0, SHA-1 44d158bc2aeefb111a620b61e043b2703e6c5808 (docs/GAME.md).
        .rom_hash = 0x41CAB16167C1368CULL,
        .internal_name = "ROBOTRON-64",
        .display_name = "Robotron 64",
        .game_id = u8"robotron64.us",
        .mod_game_id = "robotron64",
        // The game only saves to the Controller Pak (docs/GAME.md); no cartridge save hardware.
        .save_type = recomp::SaveType::None,
        .is_enabled = true,
        .decompression_routine = nullptr,
        .has_compressed_code = false,
        .entrypoint_address = get_entrypoint_address(),
        .entrypoint = recomp_entrypoint,
    },
};

// Until the game starts, the VI thread queues a dummy workload per VI (ultramodern/src/events.cpp) and
// RecompFrontend draws each with a full RT64 sync (rt64_render_context.cpp). A backlog delays the first display
// lists after Start (docs/NOTES.md), and these frames only keep the window alive, so forward at most one per 100 ms.
class ThrottledRenderContext : public ultramodern::renderer::RendererContext {
public:
    explicit ThrottledRenderContext(std::unique_ptr<ultramodern::renderer::RendererContext> inner)
        : inner(std::move(inner)) {}

    bool valid() override { return inner->valid(); }
    ultramodern::renderer::SetupResult get_setup_result() const override { return inner->get_setup_result(); }
    ultramodern::renderer::GraphicsApi get_chosen_api() const override { return inner->get_chosen_api(); }
    bool update_config(const ultramodern::renderer::GraphicsConfig& old_config, const ultramodern::renderer::GraphicsConfig& new_config) override {
        return inner->update_config(old_config, new_config);
    }
    void enable_instant_present() override { inner->enable_instant_present(); }
    void send_dl(const OSTask* task) override { inner->send_dl(task); }
    void send_dummy_workload(uint32_t fb_address) override {
        auto now = std::chrono::steady_clock::now();
        if (now - last_dummy < std::chrono::milliseconds(100)) {
            return;
        }
        last_dummy = now;
        inner->send_dummy_workload(fb_address);
    }
    void update_screen() override { inner->update_screen(); }
    void shutdown() override { inner->shutdown(); }
    uint32_t get_display_framerate() const override { return inner->get_display_framerate(); }
    float get_resolution_scale() const override { return inner->get_resolution_scale(); }

private:
    std::unique_ptr<ultramodern::renderer::RendererContext> inner;
    std::chrono::steady_clock::time_point last_dummy{};
};

namespace robotron64 {
    // Thread IDs from the decomp: src/boot/startup.c, src/boot/scheduler.c,
    // src/game/audio_startup.c, src/game/controller_service_setup.c.
    std::string get_game_thread_name(const OSThread* t) {
        std::string name = "[Game] ";

        switch (t->id) {
            case 0:
                name += t->priority == 150 ? "PIMGR" : "VIMGR";
                break;
            case 1:
                name += "BOOT";
                break;
            case 3:
                name += "MAIN";
                break;
            case 8:
                name += "CONTROLLER";
                break;
            case 17:
                name += "SCHED_17";
                break;
            case 18:
                name += "SCHED_18";
                break;
            case 19:
                name += "SCHED_19";
                break;
            case 110:
                name += "AUDIO";
                break;
            default:
                name += std::to_string(t->id);
                break;
        }

        return name;
    }
}

#ifdef _WIN32

struct PreloadContext {
    HANDLE handle;
    HANDLE mapping_handle;
    SIZE_T size;
    PVOID view;
};

bool preload_executable(PreloadContext& context) {
    wchar_t module_name[MAX_PATH];
    GetModuleFileNameW(NULL, module_name, MAX_PATH);

    context.handle = CreateFileW(module_name, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (context.handle == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Failed to load executable into memory!");
        context = {};
        return false;
    }

    LARGE_INTEGER module_size;
    if (!GetFileSizeEx(context.handle, &module_size)) {
        fprintf(stderr, "Failed to get size of executable!");
        CloseHandle(context.handle);
        context = {};
        return false;
    }

    context.size = module_size.QuadPart;

    context.mapping_handle = CreateFileMappingW(context.handle, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (context.mapping_handle == nullptr) {
        fprintf(stderr, "Failed to create file mapping of executable!");
        CloseHandle(context.handle);
        context = {};
        return false;
    }

    context.view = MapViewOfFile(context.mapping_handle, FILE_MAP_READ, 0, 0, 0);
    if (context.view == nullptr) {
        fprintf(stderr, "Failed to map view of of executable!");
        CloseHandle(context.mapping_handle);
        CloseHandle(context.handle);
        context = {};
        return false;
    }

    DWORD pid = GetCurrentProcessId();
    HANDLE process_handle = OpenProcess(PROCESS_SET_QUOTA | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (process_handle == nullptr) {
        fprintf(stderr, "Failed to open own process!");
        CloseHandle(context.mapping_handle);
        CloseHandle(context.handle);
        context = {};
        return false;
    }

    SIZE_T minimum_set_size, maximum_set_size;
    if (!GetProcessWorkingSetSize(process_handle, &minimum_set_size, &maximum_set_size)) {
        fprintf(stderr, "Failed to get working set size!");
        CloseHandle(context.mapping_handle);
        CloseHandle(context.handle);
        context = {};
        return false;
    }

    if (!SetProcessWorkingSetSize(process_handle, minimum_set_size + context.size, maximum_set_size + context.size)) {
        fprintf(stderr, "Failed to set working set size!");
        CloseHandle(context.mapping_handle);
        CloseHandle(context.handle);
        context = {};
        return false;
    }

    if (VirtualLock(context.view, context.size) == 0) {
        fprintf(stderr, "Failed to lock view of executable! (Error: %08lx)\n", GetLastError());
        CloseHandle(context.mapping_handle);
        CloseHandle(context.handle);
        context = {};
        return false;
    }
    
    return true;
}

void release_preload(PreloadContext& context) {
    VirtualUnlock(context.view, context.size);
    CloseHandle(context.mapping_handle);
    CloseHandle(context.handle);
    context = {};
}

#elif defined(__linux__) || defined(__APPLE__)

struct PreloadContext {

};

bool preload_executable(PreloadContext& context) {
    // Preloading isn't implemented on Linux and MacOS, but it's also unnecessary there, as the OS already preloads the executable.
    // Therefore, we can just consider the executable to be preloaded.
    return true;
}

void release_preload(PreloadContext& context) {
}

#else

struct PreloadContext {};

bool preload_executable(PreloadContext& context) {
    return false;
}

void release_preload(PreloadContext& context) {
}

#endif

void enable_texture_pack(recomp::mods::ModContext& context, const recomp::mods::ModHandle& mod) {
    recompui::renderer::enable_texture_pack(context, mod);
}

void disable_texture_pack(recomp::mods::ModContext&, const recomp::mods::ModHandle& mod) {
    recompui::renderer::disable_texture_pack(mod);
}

void reorder_texture_pack(recomp::mods::ModContext&) {
    recompui::renderer::trigger_texture_pack_update();
}

void on_launcher_init(recompui::LauncherMenu *menu) {
    auto game_options_menu = menu->init_game_options_menu(
        supported_games[0].game_id,
        supported_games[0].mod_game_id,
        supported_games[0].display_name,
        supported_games[0].thumbnail_bytes,
        recompui::GameOptionsMenuLayout::Center
    );
    game_options_menu->add_default_options();
    // The centre layout sits 25% from the bottom (RecompFrontend ui_launcher.cpp); lower it to leave room for
    // the hero's head and the wordmark above (launcher_animation.cpp).
    game_options_menu->set_bottom(7.0f, recompui::Unit::Percent);
    // Half of upstream's 50% width, so the items' click area hugs the text.
    game_options_menu->set_width(25.0f, recompui::Unit::Percent);
    // No highlight box behind the hovered or focused item, like the game's menu: the pulsing text colour marks it.
    for (recompui::GameOption* option : game_options_menu->get_options()) {
        option->hover_style.set_background_color(recompui::theme::color::Transparent);
        option->focus_style.set_background_color(recompui::theme::color::Transparent);
    }

    robotron64::launcher_animation_setup(menu);
}

#define REGISTER_FUNC(name) recomp::overlays::register_base_export(#name, name)

#if defined(__ANDROID__)
// SDLActivity looks this symbol up in libRobotron64.so. MainActivity.java has unpacked the assets and set
// APP_FOLDER_PATH; assets and the controller database are found relative to the working directory.
extern "C" int SDL_main(int argc, char** argv) {
    if (const char* app_folder = getenv("APP_FOLDER_PATH")) {
        std::filesystem::current_path(app_folder);
    }
#else
int main(int argc, char** argv) {
#endif
    recomp::Version project_version{};
    if (!recomp::Version::from_string(version_string, project_version)) {
        ultramodern::error_handling::message_box(("Invalid version string: " + version_string).c_str());
        return EXIT_FAILURE;
    }

    // Map this executable into memory and lock it, which should keep it in physical memory. This ensures
    // that there are no stutters from the OS having to load new pages of the executable whenever a new code page is run.
    PreloadContext preload_context;
    bool preloaded = preload_executable(preload_context);

    if (!preloaded) {
        fprintf(stderr, "Failed to preload executable!\n");
    }

#ifdef _WIN32
    timeBeginPeriod(1);

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--show-console") == 0)
        {
            if (GetConsoleWindow() == nullptr)
            {
                AllocConsole();
                freopen("CONIN$", "r", stdin);
                freopen("CONOUT$", "w", stderr);
                freopen("CONOUT$", "w", stdout);
            }

            break;
        }
    }

    SetConsoleOutputCP(CP_UTF8);

    // Change to a font that supports Japanese characters
    CONSOLE_FONT_INFOEX cfi;
    cfi.cbSize = sizeof cfi;
    cfi.nFont = 0;
    cfi.dwFontSize.X = 0;
    cfi.dwFontSize.Y = 16;
    cfi.FontFamily = FF_DONTCARE;
    cfi.FontWeight = FW_NORMAL;
    wcscpy_s(cfi.FaceName, L"NSimSun");
    SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &cfi);
#endif

#ifdef _WIN32
    // Force wasapi on Windows, as there seems to be some issue with sample queueing with directsound currently.
    SDL_setenv("SDL_AUDIODRIVER", "wasapi", true);
#endif

#if !defined(__ANDROID__)
    NFD_Init();
#endif

    recompui::programconfig::set_program_name(robotron64::program_name);
    recompui::programconfig::set_program_id(robotron64::program_id);
    
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (!reset_audio(48000)) {
        // It is not possible to initialize without an audio device.
        return EXIT_FAILURE;
    }

    std::u8string controller_db_path = (recompui::file::get_program_path() / "recompcontrollerdb.txt").u8string();
    if (SDL_GameControllerAddMappingsFromFile(reinterpret_cast<const char *>(controller_db_path.c_str())) < 0) {
        fprintf(stderr, "Failed to load controller mappings: %s\n", SDL_GetError());
    }

    recompui::register_primary_font("InterVariable.ttf", "Inter Variable");
    // Launcher wordmark (launcher_animation.cpp). SIL OFL 1.1, see assets/Orbitron-OFL.txt.
    recompui::register_extra_font("Orbitron.ttf");

    recomp::register_config_path(recompui::file::get_app_folder_path());

    for (const auto& game : supported_games) {
        recomp::register_game(game);
    }

    REGISTER_FUNC(recomp_get_window_resolution);
    REGISTER_FUNC(recomp_get_target_aspect_ratio);
    REGISTER_FUNC(recomp_get_target_framerate);
    recompui::register_ui_exports();
    recomputil::register_data_api_exports();
    recomptheme::set_custom_theme();

    robotron64::register_game_overlays();
    robotron64::register_game_patches();

    recompinput::players::set_single_player_mode(true);

    robotron64::init_config();

    recompui::register_launcher_init_callback(on_launcher_init);
    recompui::register_launcher_update_callback(robotron64::launcher_animation_update);

    recomp::rsp::callbacks_t rsp_callbacks{
        .get_rsp_microcode = get_rsp_microcode,
    };

    ultramodern::renderer::callbacks_t renderer_callbacks{
        .create_render_context = [](uint8_t* rdram, ultramodern::renderer::WindowHandle window_handle, bool developer_mode) {
            auto presentation_mode = ultramodern::renderer::PresentationMode::PresentEarly;
            std::unique_ptr<ultramodern::renderer::RendererContext> context = std::make_unique<ThrottledRenderContext>(
                recompui::renderer::create_render_context(rdram, window_handle, presentation_mode, developer_mode));
            return context;
        },
    };

    ultramodern::gfx_callbacks_t gfx_callbacks{
        .create_gfx = create_gfx,
        .create_window = create_window,
        .update_gfx = update_gfx,
    };

    ultramodern::audio_callbacks_t audio_callbacks{
        .queue_samples = queue_samples,
        .get_frames_remaining = get_frames_remaining,
        .set_frequency = set_frequency,
    };

    ultramodern::input::callbacks_t input_callbacks{
        .poll_input = poll_inputs,
        .get_input = get_n64_input,
        .set_rumble = set_rumble,
        .get_connected_device_info = get_connected_device_info,
    };

    ultramodern::events::callbacks_t thread_callbacks{
        .vi_callback = recompinput::update_rumble,
        .gfx_init_callback = nullptr,
    };

    ultramodern::error_handling::callbacks_t error_handling_callbacks{
        .message_box = recompui::message_box,
    };

    ultramodern::threads::callbacks_t threads_callbacks{
        .get_game_thread_name = robotron64::get_game_thread_name,
    };

    // Register the texture pack content type with rt64.json as its content file.
    recomp::mods::ModContentType texture_pack_content_type{
        .content_filename = "rt64.json",
        .allow_runtime_toggle = true,
        .on_enabled = enable_texture_pack,
        .on_disabled = disable_texture_pack,
        .on_reordered = reorder_texture_pack,
    };
    auto texture_pack_content_type_id = recomp::mods::register_mod_content_type(texture_pack_content_type);

    // Register the .rtz texture pack file format with the previous content type as its only allowed content type.
    recomp::mods::register_mod_container_type("rtz", std::vector{ texture_pack_content_type_id }, false);

    recomp::start({
        .argc = argc,
        .argv = argv,
        .project_version = project_version,
        .window_handle = {},
        .rsp_callbacks = rsp_callbacks,
        .renderer_callbacks = renderer_callbacks,
        .audio_callbacks = audio_callbacks,
        .input_callbacks = input_callbacks,
        .gfx_callbacks = gfx_callbacks,
        .events_callbacks = thread_callbacks,
        .error_handling_callbacks = error_handling_callbacks,
        .threads_callbacks = threads_callbacks,
    });

#if !defined(__ANDROID__)
    NFD_Quit();
#endif

    if (preloaded) {
        release_preload(preload_context);
    }

#ifdef _WIN32
    timeEndPeriod(1);
#endif

    return EXIT_SUCCESS;
}
