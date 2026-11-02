# 009 — Android: system file picker for the ROM and mods

Date: 2026-11-02. Status: accepted.

## Context
RecompFrontend's file dialog uses nativefiledialog, which has no Android backend, so ADR 007 stubbed it out. The launcher's ROM
button and "Install Mods" did nothing, and both ROMs and mods needed `adb push`. The goal is to set up the game on the phone alone.

## Options
1. **Block in the dialog function until the picker returns,** as desktop does. The picker puts the activity in the background,
   and the thread waiting could be the one that has to handle SDL's pause and resume.
2. **Store the callback and deliver the result later** from `update_gfx`. The frontend's callers already allow this, because
   macOS runs the dialog from the main queue (`recompui/src/util/file.cpp`, `support_apple.mm`).

## Decision
Option 2. `MainActivity.java` opens `ACTION_OPEN_DOCUMENT`, which needs no storage permission, and copies the chosen `content://`
files to `cacheDir/picked/<index>/<name>`, since `recomp::select_rom` and the mod installer read paths.
`src/main/android_files.cpp` runs the stored callback on the SDL event thread, where desktop drag and drop also calls
`recompui::drop_files` (`recompinput/src/input_events.cpp`), then deletes the copies. The RecompFrontend patch's two stubs now call
`robotron64_pick_files`, which the game defines.

The file name comes from the document provider and the installer uses it, so only its last path segment is kept.

## Resume had to be fixed first
The picker puts the activity in the background, and Android destroys the surface when that happens. Nothing gave RT64 the new
one, so the screen stayed black after any return to the app, with the game still running. We had the Goemon port's plume patches
(`setRenderWindow`) but not the code in its app that calls them. RecompFrontend patch 0002 adds that part, modelled on Goemon's
`src/game/input.cpp` and `src/main/rt64_render_context.cpp`: the app pauses on `SDL_APP_WILLENTERBACKGROUND`; on
`SDL_APP_DIDENTERFOREGROUND` the new window is published and the app unpauses; `update_screen` passes the window on, on the
graphics thread.

A few `No such device` lines remain in the log at the moment the app leaves the foreground. They are presents already in flight
(the "known bounded window" comment in `plume_vulkan.cpp`) and they stop by themselves.

## Tested
By the user on the S26 Ultra: wrong file shows the error and the launcher returns; correct ROM is stored and survives closing the
app; Home during gameplay comes back with a picture; a `.rtz` from Downloads installs and shows in the Mods menu. Log check:
`cache/picked` is gone afterwards. Not tested: the overwrite prompt, picking several files, Drive, and the pack's look in game.

## Consequences
- ROM validation and mod installing are the same code as on desktop.
- Only single files can be picked, so a texture pack has to be a `.rtz` or a zip (`tools/texture_pack.ps1 -Pack`), not a folder.
- "Open Mods Folder" still does nothing useful on Android: it takes the `__linux__` branch (`xdg-open`). RT64's inspector dialogs
  stay stubbed.
- No progress display during the copy.
- The patch now needs a symbol from the game, so RecompFrontend can't link alone on Android. It never does in this build.
