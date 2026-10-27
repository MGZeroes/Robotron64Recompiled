# Notes

Running log, newest at top. Failed attempts stay in.

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
