# 007 — Android test build: keep our pins, carry patch files

Date: 2026-11-01. Status: accepted for the test build, on branch `android`.

## Context
The user wants to know if the port can run on Android, testing on their own phone with a controller. This is a
go/no-go test, not a release. N64Recomp has no upstream Android target. Community ports exist: Goemon
(`ogdanimal/Goemon64Recomp-Android`), Zelda (`linkzenic/Zelda64Recomp-Android`) and Wave Race
(`HLLArtyParty/WaveRace64RecompForAndroid`).

Test device, read with `adb` on 2026-11-01: Galaxy S26 Ultra SM-S948B, SM8850, Adreno 840 (Vulkan 1.4.295),
Android 17 (SDK 37), 4 KB pages, 15 GB RAM, 3120×1440. It has every Vulkan extension RT64 uses except the optional
`VK_KHR_present_wait`, and it has `dualSrcBlend`, so the Mali white screen in Goemon's notes doesn't apply here.

Goemon's forks branch from older upstream commits than our pins, so a plain diff looks huge (70 files in `rt64`).
Almost all of that is upstream work. The Android changes are few, and they were tested against our pins with
`git cherry-pick` (applying only; nothing has been built yet):

| Repo (our pin) | Commit | Result |
|---|---|---|
| `rt64` (`d9817c6`) | `25fa568` Android patches (7 files, +192/−21) | clean |
| | `3759a70` Select DXC by host | after the one above: 1 CMake hunk conflicts, trivial (keep our `__PRFCHWINTRIN_H`) |
| | `1c7d79d` display list guard, `094d073` vsync option | clean |
| `plume` (`d890ac8`) | all 6 `ogdanimal` commits, including surface rebuild on resume | clean |
| `N64ModernRuntime` (`cdf5abb`) | `bc53b8c` Android patches (app pause), `920d493`, `cf831e1` | clean |
| | `089f12f` FramebufferEffects option | conflicts in `config.hpp`; leave out unless the driver needs it |

Goemon doesn't use RecompFrontend, so nothing covers it. It needs its own changes.

## Options
1. **Switch submodules to Goemon's forks.** Loses upstream work our pins have, and changes three pins for a test.
2. **Our own forks with the commits on top.** Clean history, but new repos to maintain for a result we don't have yet.
3. **Keep our pins, carry patch files** in `tools/android/patches/`, applied to a gitignored copy of `lib/` at build
   time.

## Decision
Option 3. Submodule pins don't change, and desktop builds aren't affected. If the test is a go, revisit this for a
real port; forks are likely the better long-term form (rule 3).

Licences allow it: `rt64` and `plume` are MIT, `N64ModernRuntime` and both ports' apps are GPL-3.0, this repo is
GPL-3.0 (GitHub API, 2026-11-01). Anything used gets credited in `CREDITS.md`.

## Result
Go on this device (2026-11-01). On the S26 Ultra's stock driver the game reached a wave in the first launch. The user
tested it with an Xbox controller and reports it "worked perfectly, like on Windows", in reply to a request to check
sound, saving and resume; each was not confirmed separately. No save file existed after that test, so saving was tested
again and works: save, restart, load (2026-11-02). Landscape only, by the user's choice. Frame rate was not measured. Only
this device has been tested.

## Consequences
- Patch files need refreshing when a pin moves.
- The Mali fallback commits are left out, so this build isn't expected to work on Mali phones.
- The Samsung fixes listed in Zelda's release notes (0.6.3, 0.6.4) have no details yet. Look at them if the phone
  fails at startup or on ROM reads.
- The CMake and RecompFrontend changes turned out small: `if (ANDROID)` branches and one RecompFrontend patch.
