# 003 — Build the decomp ELF in Docker, not a WSL distro

Date: 2026-10-27. Status: accepted.

## Context
The decomp needs x86-64 Linux (IDO 5.3 recomp binaries, `binutils-mips-linux-gnu`, GNU make,
Python ≥ 3.12). The host has no general WSL distro (only `docker-desktop`), but Docker Desktop
29.7.2 with a Linux engine is installed and running.

## Options
1. Install Ubuntu in WSL2 and set it up by hand.
2. Run the build in an `ubuntu:24.04` container from a Dockerfile in this repo.

## Decision
Option 2. A Dockerfile pins the distro and package set, so the ELF build is reproducible on any
machine with Docker (and in CI) without hand-configured state. A thin `tools/build_elf.ps1`
wrapper runs it from Windows; `tools/build_elf.sh` is the in-container entry point.

## Consequences
- CLAUDE.md "run in WSL2" guidance is replaced by "run in the Docker build image".
- Bind-mounted Windows paths are slower than a native Linux filesystem; acceptable for a
  one-shot build. If it becomes a problem, copy the decomp into a Docker volume instead.
