// The system file picker on Android (ADR 009), behind RecompFrontend's file dialog. The picker puts the activity in the
// background, so nothing waits for it here: the callback is stored, and update() runs it once MainActivity.java has
// copied the chosen files to the cache.

#include <filesystem>
#include <functional>
#include <list>
#include <mutex>
#include <string_view>
#include <system_error>
#include <utility>

#include <jni.h>

#include "SDL.h"

#include "robotron64_files.h"

namespace {
    using PickCallback = std::function<void(bool, const std::list<std::filesystem::path>&)>;

    std::mutex pick_mutex;
    PickCallback pick_callback;
    bool pick_done = false;
    std::filesystem::path pick_folder;
    std::list<std::filesystem::path> pick_paths;

    // Through UTF-16: GetStringUTFChars gives modified UTF-8, which is wrong for characters such as emoji in a file name.
    std::filesystem::path to_path(JNIEnv* env, jstring string) {
        const jchar* chars = env->GetStringChars(string, nullptr);
        std::filesystem::path path{std::u16string_view{reinterpret_cast<const char16_t*>(chars), static_cast<size_t>(env->GetStringLength(string))}};
        env->ReleaseStringChars(string, chars);
        return path;
    }
}

// The RecompFrontend patch calls this from its file dialog functions.
void robotron64_pick_files(bool multiple, PickCallback callback) {
    {
        std::lock_guard lock{pick_mutex};
        if (!pick_callback) {
            pick_callback = std::move(callback);
            callback = nullptr;
        }
    }
    if (callback) {
        callback(false, {});
        return;
    }

    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    jclass activity_class = env->GetObjectClass(activity);
    env->CallVoidMethod(activity, env->GetMethodID(activity_class, "pickFiles", "(Z)V"), static_cast<jboolean>(multiple));
    env->DeleteLocalRef(activity_class);
    env->DeleteLocalRef(activity);
}

void robotron64::files::update() {
    PickCallback callback;
    std::filesystem::path folder;
    std::list<std::filesystem::path> paths;
    {
        std::lock_guard lock{pick_mutex};
        if (!pick_done) {
            return;
        }
        pick_done = false;
        // The callback may start the next pick, so the slot has to be free before it runs.
        callback = std::exchange(pick_callback, nullptr);
        folder = std::exchange(pick_folder, {});
        paths = std::exchange(pick_paths, {});
    }

    callback(!paths.empty(), paths);

    // Both users are done with the files by now: the ROM is stored and mods are in the mods folder, even if an overwrite
    // prompt is still open.
    if (!folder.empty()) {
        std::error_code ec;
        std::filesystem::remove_all(folder, ec);
    }
}

extern "C" {
    // folder and paths are null when the pick was cancelled or failed.
    JNIEXPORT void JNICALL Java_io_github_mgzeroes_robotron64_MainActivity_nativePickDone(JNIEnv* env, jclass, jstring folder, jobjectArray paths) {
        std::filesystem::path folder_path;
        std::list<std::filesystem::path> picked;
        if (folder != nullptr) {
            folder_path = to_path(env, folder);
            for (jsize i = 0; i < env->GetArrayLength(paths); i++) {
                jstring path = static_cast<jstring>(env->GetObjectArrayElement(paths, i));
                picked.emplace_back(to_path(env, path));
                env->DeleteLocalRef(path);
            }
        }

        std::lock_guard lock{pick_mutex};
        pick_folder = std::move(folder_path);
        pick_paths = std::move(picked);
        pick_done = true;
    }
}
