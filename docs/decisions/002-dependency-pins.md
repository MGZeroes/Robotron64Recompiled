# 002 — Dependency pins

Date: 2026-10-27. Status: accepted.

## Context
CLAUDE.md rule 3: every upstream dependency is a submodule at an explicit commit, with the
reason recorded here.

## Decision

| Submodule | Commit | Date | Why |
|---|---|---|---|
| `lib/N64ModernRuntime` (contains N64Recomp) | `cdf5abbd5026fef5c364c676e4667c45e42b6863` | 2026-08-30 | upstream `main` head; newer than the template's `ca568b6` (2026-01-29); body-harvest-recomp builds on this same commit with RecompFrontend |
| `lib/rt64` | `d9817c68ffd7328cd0af383af124971489366e18` | 2026-10-23 | upstream `main` head |
| `lib/RecompFrontend` | `2ddbfc8adcfe9ba26520441f14488861fdcefef7` | 2026-10-16 | upstream `main` head (license commit on top of `b1a1477`, last code change 2026-08-15) |
| `decomp` (frankischilling/robotron64) | `812a75f85ca925d8752a8a92c21acbdeab862d9f` | 2026-10-11 | decomp `main` head at project start; byte-matching build |

N64Recomp is the nested submodule pinned by N64ModernRuntime `cdf5abb`. That is
`81213c1831fab2521a6a5459c67b63437d67e253` (2026-01-17, "Update to fmt-12.1.0 to fix build issue with clang-21"),
which is two commits behind N64Recomp head `ffb39cd`. Those two commits are a CONTRIBUTING.md and
"dump symbols in deterministic order". We keep the runtime's pin, so the recompiler matches the runtime headers.
The `81213c1` commit message also shows that recent Clang (21+) is being tested, which is relevant for our Clang 22.1.8.

## Fallback
The template's known-good trio is N64ModernRuntime `ca568b6`, RecompFrontend `b3b7ebb`,
rt64 `f0d8c9f`. If the template's host code fails to build against the heads and the fix is
not small, drop back to that trio, record why here, and bump later as a separate step.

## Consequences
Changing any pin (especially `decomp`, per CLAUDE.md rule 5) needs a new entry here.
