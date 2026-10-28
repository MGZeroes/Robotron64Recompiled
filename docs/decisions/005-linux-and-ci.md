# 005 — Linux build in Docker; ROM-free CI

Date: 2026-10-28. Status: accepted.

## Context
Linux is the second target (CLAUDE.md). The user has no Linux desktop and no WSL distro (ADR 003), and the repo
has no GitHub remote yet.

The template's CI can't be used as it is:
- `validate*.yml` and `update-pr-artifacts.yml` check out a private repo through secrets to get the ROM and other
  inputs;
- they use the template's file names (`n_aspMain.us.toml`);
- they don't know about our decomp → symbols step;
- their artifacts would contain recompiled game code.

## Options
- **Linux:**
  - (a) build in the existing Docker image;
  - (b) set up WSL2 with WSLg to also run it;
  - (c) wait for a Linux machine.
- **CI:**
  - (a) ROM-free checks only;
  - (b) full builds with the ROM from a private repo through secrets, as the template does.

## Decision (by the user)
- **Linux (a).** `tools/build_linux.sh` builds the game in the `ubuntu:24.04` build image (SDL2 2.30, Freetype and
  GTK3 from apt). The patch ELF is also built there with clang's MIPS backend. The build counts as verified when it
  links and `ldd` resolves every library. Nobody has run it yet.
- **CI (a).** `.github/workflows/ci.yml` checks that no ROM-derived files are tracked, builds N64Recomp/RSPRecomp
  on Linux and Windows, and syntax-checks the tool scripts. The template workflows are removed. It runs once a
  GitHub remote exists.

## Consequences
- **No game build in CI:** CI never touches the ROM or produces game binaries, so there's no legal exposure.
- **Linux run untested:** runtime problems on Linux (Vulkan, input, file paths, the pak file location) stay unknown
  until someone runs it.
- **Docker image grows:** the image gains the GUI development libraries, which costs a few hundred MB.
