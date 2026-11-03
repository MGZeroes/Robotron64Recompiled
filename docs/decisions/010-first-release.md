# 010 — First release: packages, signing and going public

Date: 2026-11-02. Status: packaging accepted; publishing the builds publicly is **not decided yet**. This isn't legal advice.

## Context
The user wants a 1.0.0 release for Windows, Linux and Android, and wants the repository ready to go public. Until now no build left
the machine: ADR 005 keeps game builds out of CI, and the README said never to share the APK.

## Decisions (by the user)
- **Version 1.0.0.** Android stays labelled experimental because it has only been tested on one phone.
- **Android release key,** made by `tools/make_release_key.ps1` and stored outside the repo, with no personal details in the
  certificate. Android only accepts an update signed with the same key, so losing the key means uninstalling, which deletes the
  app's saves on the phone. The change from the debug key needs one uninstall.
- **Packages come from one clean commit,** through `tools/package_release.ps1`, into the gitignored `dist/`.
- **`CLAUDE.md` is local only and `KICKOFF_PROMPT.md` is deleted.** Both were also purged from git history, with the font below, on
  2026-11-03. That changed every commit hash.
- **Release packages come from the tagged code.** The user fully tested the packages from `80d6b5d`, a hash the purge replaced. A code
  cleanup followed, so the packages were rebuilt from the final code for a shorter re-test.

## Licences found while packaging
- **DXC:** Windows can't start without `dxil.dll` and `dxcompiler.dll`. RT64 supplies a v1.7.2212 build, and the
  `LICENSE-MS.txt` in that release's zip is a pre-release licence forbidding distribution. Packages instead use both DLLs from
  v1.8.2505.1, pinned by SHA-256. Its licence has a "Distributable Code" section. Open points in those terms: they refer to a
  "distributables file list" that isn't in the zip; they require end users to agree to terms protecting Microsoft; and they include
  an indemnity to Microsoft. Developer builds still use RT64's pair. The swap only counts as working once the Windows package has
  been play-tested.
- **Suplexmentary Comic NC font:** "All rights reserved", no licence, and nothing loads it. Removed from the tree and from history.
- **Everything else:** `tools/collect_licenses.py` copies each licence into the packages and fails if a listed file goes missing.
  The list was made by reading the source tree, so a library that keeps its notice somewhere unusual could still be missing.

## What each package contains
| Package | Contents |
|---|---|
| Windows zip | exe, `SDL2.dll`, the DXC pair, `assets/`, `recompcontrollerdb.txt`, `LICENSE`, `CREDITS.md`, `licenses/` |
| Linux tar.gz | binary, `assets/`, `recompcontrollerdb.txt`, `LICENSE`, `CREDITS.md`, `licenses/`. Needs system libraries (see README) |
| Android APK | native libraries, `assets/`, controller database and `licenses/` inside the APK, plus the release signature |

None of them contains a ROM, game assets or texture packs. A scan on 2026-11-02 compared every non-trivial 4 KB block of the ROM
(1,860 blocks) against each large binary and found no matches, so no long stretch of raw ROM data is embedded.

Linux and Android binaries are packaged without debug info; exported symbols stay. Windows has no PDB in its package. The exe still
contains 19 source paths starting `E:/RecompProjects/Robotron64/` from logging macros. They hold no personal data, so we kept them
rather than rebuild everything with path mapping. The Linux and Android binaries contain `/work/` and `/build/` paths the same way.

Requirements, read from the binaries: Windows imports the Visual C++ runtime (`MSVCP140`, `VCRUNTIME140`,
`MSVCP140_ATOMIC_WAIT`), which isn't bundled. Linux needs glibc 2.38, because it's built on Ubuntu 24.04, plus system SDL2, GTK3 and
FreeType.

## Open before builds go public
1. **Copyright.** The builds contain the game's code, translated from the ROM, even though players supply their own ROM. Other
   recomp projects publish builds, which doesn't show it's safe. The lower-risk option is a public repository with source only,
   where players build their own.
2. **GPL-3.0.** Publishing a build requires offering its corresponding source. The repository, its pinned submodules and
   `tools/android/patches/` cover the project's code. The recompiled C is also compiled into the binary and can't be published, and
   the build instructions regenerate it from the player's own ROM. Whether that's enough is unresolved.
3. **Decomp licence.** `frankischilling/robotron64` has no licence (`docs/RESEARCH.md`). It supplies symbols and function boundaries
   at build time. Symbol names may appear in unstripped binaries: check the Linux binary before publishing it.
4. **History.** Going public publishes all history too. After a force-push, GitHub can still serve the old commits by hash, including
   the purged files, and old Actions runs remain. Delete and recreate the repository with the rewritten history, or ask GitHub Support
   to purge them.

## Before making the repository public
- Re-run the history scan for secrets, personal data and ROM content.
- Delete any release packages that shouldn't be public: visibility applies to them too.
- Treat it as irreversible, since forks and copies can't be recalled.

## Consequences
- CI remains ROM-free and builds no packages (ADR 005).
- Releases are built on the maintainer's PC, so the key and ROM never reach GitHub.
- Moving the DXC pin is a manual edit of the URL and hash in `tools/package_release.ps1`.
