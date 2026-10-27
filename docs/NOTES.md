# Notes

Running log, newest at top. Failed attempts stay in.

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
