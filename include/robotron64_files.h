#ifndef __ROBOTRON64_FILES_H__
#define __ROBOTRON64_FILES_H__

// The system file picker on Android (ADR 009): MainActivity.java copies the chosen files to the cache, and the
// RecompFrontend patch gets them through robotron64_pick_files in android_files.cpp.
namespace robotron64::files {
    // Call on the thread that pumps SDL events: hands a finished pick to the waiting callback.
    void update();
}

#endif
