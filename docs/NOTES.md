# Notes

Running log, newest at top. Failed attempts stay in.

## 2026-10-28 — Widescreen background

- **Cause of the "box":**
  - The background gradient is a 3D quad at Z = 32000 with half-extents `D_8007CCA8`/`D_8007CCAC`.
    `func_800400D0` sets them every frame to 19200 × 14400 (decomp
    `renderer_surfaces/background_dispatch.c`).
  - In the game's 4:3 projection that covers about 96% of the view; on a TV the edge was hidden in overscan.
  - RT64's Expand widens the view, not the quad.
- **Not the cause:** a 2D or fade rectangle. The colour buffer is cleared every frame.
- **Fix:**
  - A hook at `0x80040190` (`L_80040190`, where both extent paths join) passes the extents through
    `robotron64_background_extent_x/y` (`ultra_extensions.cpp`).
  - These grow the quad by 6%, and in Expand also multiply X by `window aspect / (4/3)`.
  - The result is clamped to s16; that's enough up to about 2.1:1.
- **Tested by the user:** with Expand the gradient fills the window; Original still looks right.
- **Next issue:** objects near the left/right edges disappear in Expand. The game culls against its 4:3 view.

## 2026-10-28 — Controller Pak saving

- **Why:** the runtime's `osPfs*` functions are stubs that return `PFS_ERR_NOPACK`, so the game warned "NO CONTROLLER
  PAK" and couldn't save.
- **What:** `src/game/controller_pak.cpp` implements the libultra file API the game calls (GAME.md lists them) for
  channel 0. It works at the note level, not as a raw 32 KiB PFS image. That was simpler to get right without a
  reference implementation to check against. The catch: the file isn't an emulator `.mpk`, and a converter could
  be added later.
  - Storage: `<config>/saves/robotron64.us.controller_pak_1.bin`, written with the runtime's
    `open_output_file_with_backup` / `finalize_output_file_with_backup` (it keeps a `.bak`).
  - The file defines all ten `_recomp` symbols from librecomp's `pak.cpp`, so the static library never links that
    object, and there was no duplicate-symbol error. `osPfsIsPlug` and `__osGetId` moved there from
    `ultra_extensions.cpp`. IsPlug now reports a pak in port 1, and `__osGetId` returns 0.
- **Tested by the user:**
  - the warning is gone;
  - saving and loading work;
  - after quitting completely and restarting, LOAD GAME lists the save and loads it;
  - the file is about 5 KB, as expected for one 4 KiB note plus the header.

## 2026-10-28 — Analog aiming via the game's two-controller mode

**Finding.** The game already supports analog aiming (decomp `src/game/player/behavior.c`). Player 1 moves with
port 0 and, if port 1 is connected, aims with port 1's analog stick (`secondaryX/Y`). The shot angle is
`func_8003CD4C(y, x)`, which has full resolution. Held fire direction bits (D-pad/C buttons) override this with
8-way fire. Player 2 uses ports 2 and 3 the same way. This is the N64 two-controllers-per-player setup. Movement is
analog too, which corrects the previous entry: only the direction bits are 8-way.

**Change.** No game patch.
- `get_n64_input` (`src/main/main.cpp`) wraps RecompFrontend's input callback. In single-player mode, odd ports
  return only the right stick (Y negated) and no buttons.
- The right stick is suppressed from mapped inputs (`recompinput::set_right_analog_suppressed`), so it no longer
  produces C-button bits, which would override the aim.
- Option Settings → General → Analog Aiming (`src/game/config.cpp`, default On) controls both. Off restores the
  C-button mapping.

**Tested by the user:** smooth aiming at any angle; Off brings back 8-way fire; moving with only the left stick
doesn't shoot.

## 2026-10-28 — v0.2-playable: long play test passed

The user tested the build containing the changes below.
- **Passed:**
  - Start right after more than 2 minutes in the launcher, so the throttle works;
  - twin-stick and keyboard;
  - clean audio, with music switching correctly;
  - 35 minutes and 26 waves with no crash, including the bonus stage, game over and pause/resume;
  - password `BQJDNQMBHC` (wave 21).
- **Framerate setting:** no visible change at any value. That's expected, because RT64 needs interpolation
  patches to show more frames than the game renders.
- **Right stick fires in 8 directions only.** This is the original design: `func_8003C1A8` reduces every input to
  direction bits (GAME.md, Input). Analog aiming would be a gameplay patch, behind an option; it's next.
- **Transition trails feel inconsistent.** Deferred.

## 2026-10-28 — Play-tested to wave 5; triage and small fixes

**Play test (user).**
- Waves 1–5 with no crash or hang; the controls feel right.
- The user compared Original and Expand settings with N64 footage of the Crave intro, the "PLAYER 1 UP"
  transition and wave 1.
- At Original settings the picture matches the N64.
- The 4:3 "box" with Expand, and the smeared transitions, are explained in GAME.md (Rendering). Both look like the
  original game's own behaviour, so they're deferred.

**Changes.**
- **Slow Start after a long stay in the launcher.** `ThrottledRenderContext` in `src/main/main.cpp` wraps
  RecompFrontend's render context. It forwards at most one `send_dummy_workload` every 100 ms and drops the rest,
  so a backlog of dummy frames can no longer delay the first game display lists. Nothing in RecompFrontend casts
  the context back to `RT64Context` (grep), so the wrapper is safe.
- **Controls.** The C buttons are labelled "Fire … (right stick)". RecompFrontend's default mapping already puts
  them on the right stick (`recompinput/src/input_mapping.cpp:44-47`), so twin-stick works without new code.
- **Log.** The diagnostic `printf` is gone from the `osAiGetLength` clamp hook.
- **Docs.** README (controls, recommended settings) and the new `docs/TESTING.md` checklist.

**Not changed: the default graphics settings.** Expand and Display are hard-coded in
`RecompFrontend/recompui/src/config/ui_config_tab_graphics.cpp:291,307`, and no setter was found. The README
recommends Original instead.

## 2026-10-27 — ONE PLAYER starts: three runtime timing fixes

The user confirmed that ONE PLAYER now works. Each problem was diagnosed from evidence: WER crash records,
`llvm-symbolizer` on the faulting RVA, and thread stacks from WinDbg's `cdbX64 -pv` (a non-invasive attach). LLVM's
`lldb` can't start here because it needs `python311.dll`. All fixes are `[[patches.hook]]`s in
`robotron64.us.toml`.

1. **Crash pressing Start (sometimes).** `0xC0000005` in `func_80050BD0` (graphics scheduler thread).
   - Cause: it dereferences `D_8014BE40`, the profiling slot that the retrace thread sets on the first VI
     (decomp `scheduler.c:79-87`). On hardware a VI always comes first; under the runtime a task can.
     The audio scheduler (`func_80050928`) has the same dereference.
   - **Failed first fix:** a hook that waited with `yield_self_1ms` until the pointer was set *deadlocked* at
     Start (all game threads idle).
   - Final fix: if it's NULL, point it at slot 0 (`0x8014B9E0`), which is what the first retrace does anyway.
2. **Crash a little later.** `0xC0000005` in host `queue_samples` (`main.cpp`).
   - Cause: the game sizes each audio frame as `D_801901F4 - osAiGetLength()/4 + D_8008D834`
     (`audio_task_build.c`). The runtime reports the host audio queue, which is much larger than real hardware
     would report (logged: up to 6821 bytes against 2112), so the result wraps to about 0xFFF0 frames.
   - Fix: clamp the value `osAiGetLength` returns, in `func_800521C8`. As a backstop, `queue_samples` also drops
     chunks over 0x8000 samples.
3. **Freeze on ONE PLAYER.** The WinDbg stacks showed every game thread in `osRecvMesg` except MAIN, which was
   executing in `func_80053CC0` called from `func_80051680`.
   - Cause: a busy-wait for the audio thread to stop the old music, `while (func_80053CC0(id) == 3) {}`
     (`audio_generation.c:61-64`). Game threads are scheduled cooperatively, so the audio thread never runs.
   - Fix: `yield_self` in the loop.

**Known issue: a slow Start after a long time in the launcher.**
- Stacks showed the host Gfx thread working through `send_dummy_workload` (RecompFrontend), with a `fullSync`
  each time, while MAIN waited for its first graphics task.
- The VI thread queues one dummy workload per VI while the launcher is shown. They pile up when processing is
  slower than 60/s, and the delay after Start grows with launcher time. It doesn't hang; it drains eventually.
- To fix in our code (not the submodules), e.g. by wrapping the render context to skip stale dummy workloads.
  (Done 2026-10-28, see above.)

## 2026-10-27 — Milestone v0.1-title: boots to the title menu

**Confirmed by the user's screenshots**
- The launcher accepts `baserom.us.z64` (XXH3 `rom_hash` matches).
- The game shows "WARNING: NO CONTROLLER PAK. THE GAME CANNOT BE SAVED / OK". That is the expected result of
  our "no pak" stubs (`osPfsIsPlug`, `__osGetId`, runtime `osPfs*`).
- Then the title menu: a rotating 3D head with ONE PLAYER / TWO PLAYER GAME / LOAD GAME / SETUP.
- RT64 renders the F3DEX/F3DLP.Rej display lists correctly, and menu input works.

**Fixed: aborts when double-clicked**
- The first attempts died with `0xC0000409` in `ucrtbase.dll`, fast-fail 7, i.e. `abort()`.
- The exe was double-clicked in `build-cmake\`. RecompFrontend resolves `assets/` and `recompcontrollerdb.txt`
  against the working directory (`recompui/src/util/file.cpp`, `get_program_path()` returns `""` on Windows).
- Fix: a POST_BUILD step copies both next to the exe. Tested: launching with cwd `build-cmake\` reaches the
  launcher, with no crash event.

**Known issue:** the game freezes after choosing ONE PLAYER. Not diagnosed yet; next step.

## 2026-10-27 — Phase 3: native Windows build links

Toolchain: VS 2022 Build Tools 17.14.37806.7, Windows SDK 10.0.26100, LLVM 22.1.8 (`clang-cl`), Ninja.
`tools/build.ps1` imports `vcvars64.bat` and builds `build-cmake/Robotron64Recompiled.exe`.

**Recomp input fixes**
- n64sym misnamed `0x80063560` as `osPiWriteIo`. The game reads ROM words with it
  (decomp `rom_files.c`), so it is `osPiReadIo`.
- `0x800617A0` is `__osGetId` (decomp `pfs_repair_id.c`); the second match at `0x80068F44` stays unnamed.
- Both are `OVERRIDES` in `gen_syms.py`. Unresolved calls are now exactly the host functions in
  `src/game/ultra_extensions.cpp`: `osPiRawReadIo` (bounds-checked; boot reads past the ROM end return 0),
  `osPfsIsPlug` (no pak), `__osGetId` (`PFS_ERR_NOPACK`).
- My unresolved-call check only scanned the runtime, not the template. The template already defines
  `osPiReadIo_recomp` (`src/game/recomp_api.cpp`), which gave a duplicate-symbol link error; mine was removed.

**Build failures, in order**
1. GNU `clang++` driver: `clang++: error: no such file or directory: '/W4'`. The template's CMake adds MSVC
   flags (`/W4`, `/SUBSYSTEM`) whenever clang simulates MSVC; it targets `clang-cl` (its `CMakeSettings.json`
   uses `clang_cl_x64`). Switched to `clang-cl`, which also accepts `-march=nehalem` etc.
2. `vcvars64.bat`: "vswhere.exe not found". It calls vswhere from PATH; added the Installer dir to PATH.
3. Patch ELF: `Unknown command line argument '-mips-ssection-threshold=0'`. **The LLVM 22.1.8 Windows release
   has no MIPS backend** (`clang --print-targets`), so no MIPS code at all.
   - Fix: the patch ELF is built in Docker by `tools/recomp.sh` (`make -C patches`, Ubuntu clang/lld).
   - CMake option `PATCHES_BUILD_ELF` (OFF in `build.ps1`) then only recompiles it with the native
     `N64RecompCLI`.
   - `make` is no longer needed on Windows. The patch steps are proper custom commands now, which also removes
     the template's "two builds needed" TODO.
4. SDL 2.30.3 `SDL_endian.h`: `definition of builtin function '_m_prefetch'`. Still the same in SDL 2.32.10.
   Clang 22 made it a builtin. Fix: define `__PRFCHWINTRIN_H` (the guard SDL checks) for clang on Windows.
5. `recomp::start` API changed with the runtime pin bump (ADR 002): it now takes one
   `recomp::Configuration`. `main.cpp` was ported; `argc`/`argv` are now passed through.

**Host glue**
- `rom_hash = 0x41CAB16167C1368C` (XXH3-64, computed with python3-xxhash in the Docker image).
- `save_type = None`, `is_enabled = true`.
- Thread names from the decomp's `osCreateThread` calls.
- Control descriptions from decomp `docs/controller-input-mapping.md`.

## 2026-10-27 — Phase 2: ELF, symbols, recompilers

**Docker image** (`tools/docker/Dockerfile`, ubuntu:24.04 + n64sym `ccf4600`). It builds in about a minute.

**Decomp build** (`tools/build_elf.ps1` → `tools/build_elf.sh`)
- `make verify`: *"Matched all 8388608 bytes; SHA-256 91d85bae…addd"*.
- `make analyze`: spimdisasm 1.42.4 produced 1,536 function records covering 0x6F040 bytes.
- First attempt "failed": Windows PowerShell 5.1 with `$ErrorActionPreference = "Stop"` turns the first native
  stderr line (an IDO `-mips3` warning) into a terminating error. The detached container kept building and
  exited 0. Fix: no Stop preference; run docker through `cmd /c ... 2>&1` and rely on the exit code.

**n64sym**
- 379 distinct addresses across a thorough run (`-t`) and a normal run (`-h 0x7FFFF400`).
- It prints most addresses as `0x70xxxxxx` and some as `0x80xxxxxx` with the same low 24 bits; normalized.
- Most hits outside function starts are data symbols (`osTvType`, `__osPiDevMgr`, …), which are useful later for
  `datasyms`.
- Hits in game code below `0x8005FBB0` are false positives on tiny functions; rejected, see ADR 004.
- **Watch list for Phase 3:** n64sym called `0x800495BC` `__osSpSetPc` and `0x8005D9E0` `__osDisableInt`.
  If these really are copies of hardware-touching code inside game code, they will misbehave when recompiled.

**gen_syms.py**
- 1,539 functions: 1,467 from the decomp, 216 libultra names from n64sym.
- Gaps total 0x520 bytes, all zero padding.
- First run failed its own check: the decomp's `func_80066A60`/`80067048`/`80067350` (`__osException`,
  `handle_CpU`, `__osCleanupThread`) were merged into neighbours by spimdisasm. Fixed by splitting at decomp
  starts.
- Ambiguous pairs: aliases resolved to their public names. `0x800669B0`/`0x80066A00` both call
  `__osSiDeviceBusy`, so they are `__osSiRawReadIo`/`__osSiRawWriteIo`.
- `__osGetId` matched twice (`0x800617A0`, `0x80068F44`), so both were left unnamed.

**Audio microcode**
- Stock `aspMain`: text ROM `0x72910`, size `0xE20`.
- The 16-entry dispatch table at data ROM `0x97BD0+0x10` is identical to the aspMain configs of drmario64_recomp
  and Quest64-Recomp.
- RSPRecomp gives `rsp/aspMain.cpp` (73 KB) with no warnings. The template's `n_aspMain` was renamed throughout.

**N64Recomp** (`tools/recomp.sh`)
- Function count 1539, exit 0. The only message is `[Info] Indirect tail call in recomp_entrypoint`
  (`_start` jumps to `0x80048170` via a register, which is resolved at runtime). 178 `LOOKUP_FUNC` sites,
  70 jump-table switches.
- Unresolved-call check against the runtime: first run had 14 missing `*_recomp` callees.
  - 12 libultra-internal functions are dead code: reachable only from reimplemented functions or each other.
    They are now `ignored` in `robotron64.us.toml` with reasons.
  - **Still unresolved, for Phase 3 to provide:**
    - `osPiRawReadIo` (boot, `func_80048170`; the reads beyond the end of the ROM);
    - `osPiWriteIo` (`func_8004EBE4`/`func_8004EE30`, to investigate);
    - `osPfsIsPlug` (5 game callers);
    - `__osCheckPackId`, `__osContRamRead`, `__osPfsSelectBank`, `__osRepairPackId`. These are called by
      `func_800617A0` (an `__osGetId`-shaped pak function called from game code via `func_8004C0B0`). Plan:
      make that path report "no pak" like the runtime's `osPfs*` stubs.

## 2026-10-27 — Phase 1: skeleton

- `git init` (branch `main`). Imported N64RecompGameTemplate `be3271c` (ADR 001) after running
  its `configure_game.py` in a scratch copy; fixed display names to "Robotron 64" by hand.
  `GameEntry.internal_name` set to the ROM header name `ROBOTRON-64` (to verify in Phase 3 how
  librecomp compares it); `rom_hash` still a placeholder until we confirm which hash librecomp uses.
- Submodules at heads (ADR 002); `git submodule update --init --recursive` succeeded.
  N64ModernRuntime pins N64Recomp `81213c1` (2 commits behind its head; fine).
- Symbol dumps go to `syms/` (gitignored). The template expected a committed `*RecompSyms` repo;
  we generate them locally from the decomp ELF instead (CLAUDE.md rule 1).
- `tools/check_rom.ps1` / `.sh`: tested on the US ROM (OK), EU ROM (mismatch, exit 1) and a
  byte-swapped copy of the US ROM (converted → OK).
- Environment: LLVM 22.1.8 at `C:\Program Files\LLVM\bin` (not on PATH; has clang, clang-cl,
  ld.lld, llvm-objcopy). **No Visual Studio / Windows SDK installed** — needed for linking on
  Windows (user installing VS 2022 Build Tools). No WSL distro; Docker Desktop 29.7.2 available →
  decomp builds in Docker (ADR 003).

## 2026-10-27 — Phase 0: research

See [RESEARCH.md](RESEARCH.md). Key outcomes:
- No existing Robotron 64 recomp.
- First local ROM (`285c720f…`) was a bad dump: same header/CRCs, 227 bytes differ in the
  asset area `0x4726BC`–`0x47ADFF`. User supplied the correct dump (`44d158bc…`, matches decomp).
- Decomp builds a matching ELF + map but ~128 KB of code is unmatched `.incbin` with no
  symbols — main technical risk for Phase 2.
