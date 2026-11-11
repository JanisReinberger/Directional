# AGENTS.md — Directional

## Project
**Directional** — header-only C++ library for creating, manipulating, and visualizing
directional fields on 3D meshes (SGP 2021 software award). Upstream: `avaxman/Directional`.

## Tech stack
- C++20, header-only (`include/directional/`); one header ≈ one function, snake_case names
- Eigen and Polyscope bundled as submodules; optional GMP (Mesher package only, works without it)
- Build: CMake ≥ 3.20 + Ninja for the tutorials; docs: MkDocs (Python)

## Package manager + commands
No JS package manager; C++ dependencies are git submodules. Docs env via `Pipfile`/`requirements.txt` (conda-style).

- Fetch deps: `git clone --recursive https://github.com/avaxman/Directional.git`
- Configure: `cmake -S tutorial -B tutorial/build -G Ninja`
- Build: `cmake --build tutorial/build -j6` → binaries in `tutorial/bin/` (e.g. `101_GlyphRendering_bin`)
- Docs: `mkdocs serve` / `mkdocs build` (published to the `gh-pages` branch)
- Test: not configured (a `googletest` submodule exists but no test target is wired up)
- Lint: not configured (old `scripts-deprecated/pre-commit.sh` is deprecated)

## Repository structure
- `include/directional/` — the header-only library (add to include path, then `#include <directional/foo.h>`)
- `tutorial/` — numbered chapter tutorials `NNN_Topic/` with `main.cpp` + own CMakeLists; output to `tutorial/bin/`
- `external/` — submodules: `eigen`, `polyscope`, `googletest`
- `docs/` — MkDocs source (`mkdocs.yml`); `site/`, `style.css`, `sitemap.xml.gz` — website assets
- `optional/` — legacy libigl-style optional components
- `scripts-deprecated/` — old shell scripts, not maintained

## CI
None detected (no `.github/` workflows).

## Branching & PR workflow (strict runbook — agents must follow this)

Remotes:
- `upstream` — `avaxman/Directional`, the original library. PRs target its `master`. Read-only for us.
- `origin` — the fork (`JanisReinberger/Directional`): backup of `dev` + source of PR branches.

Branches:
- `master` — pristine mirror of `upstream/master`. **Never commit here, never force-push.**
  Update only by fast-forward:
  `git fetch upstream && git checkout master && git merge --ff-only upstream/master`
- `dev` — your working branch = `master` + env commits (AGENTS.md, `.sandbox/SETUP_NOTES.md`,
  `.gitignore` tweaks, local build fixes) + any work in progress. Agent sessions run on this branch.
  PR branches are extracted from it via cherry-pick — never merged into it.
  Back it up (operator pushes; agents never push): `git push origin dev`
- `feature/<name>` — ephemeral PR branch = `master` + **only** the improvement commits.
  `dev` and `feature/*` are **siblings**: neither may ever become an ancestor of the other.
  The same change may exist on both (dev holds a cherry-picked copy with a new SHA) — that is
  fine; only the `feature/*` branch is ever pushed / PR'd.
  Current: `feature/rounding` (epsilonRound + roundingOffset; `git diff master...feature/rounding`
  touches only `include/directional/{integrate,setup_integration}.h` and the 503/505 tutorials).

### Opening a PR
1. Independent feature: `git checkout -b feature/<name> master`
   Stacked on an open PR (uses its helpers): `git checkout -b feature/<name> feature/<parent>`
2. Bring in the work: `git cherry-pick <shas>` from `dev` (or from the parent feature branch).
   To continue an open PR's work on `dev`: `git checkout dev && git cherry-pick <shas-from-feature-branch>`.
3. Guardrail — must list only library/tutorial files, never env files:
   `git diff master...feature/<name> --stat`
4. Operator pushes and opens the PR against `upstream/master`: `git push -u origin feature/<name>`

### NEVER
- Merge `feature/*` into `dev`, or reset/move `dev` onto a `feature/*` branch — dev stays a
  *sibling* of feature branches, never their descendant. To get a PR branch's commits onto dev,
  `git cherry-pick` them (new SHAs) — never `git merge`. (This mistake happened once; fixed by
  rebuilding dev from master.)
- Commit on `master`; force-push anything except your own `feature/<name>` after review fixes
- Run `git filter-branch` / `filter-repo` on anything that has been pushed
- Push from inside an agent sandbox (the operator pushes when they choose to)

### When upstream moves / a PR merges
- `git fetch upstream && git checkout master && git merge --ff-only upstream/master`
- `git rebase --onto master <old-master-sha> dev` — env commits replay cleanly; feature commits
  that landed upstream with the same patch drop out automatically. If upstream squash-merged and
  the rebase conflicts, recreate dev instead: `git checkout -b dev2 master && git cherry-pick <env-shas>`
- `git branch -D feature/<name>`
- Stacked PRs shrink automatically: `git rebase --onto master <parent-pr-tip> feature/<child>`

### Pulling an open PR's review fixes back onto dev
`git checkout dev && git cherry-pick <new-shas>` — cherry-pick, never merge.

Housekeeping: `sandbox/*` branches and `refs/pi-publish/*` are agent-harness artifacts — ignore them.

## Conventions
- Header-only library: each header generally provides one function; C++20 (`CMAKE_CXX_STANDARD 20`)
- Tutorial chapters named `NNN_Topic`, one executable per chapter named `NNN_Topic_bin`
- Polyscope `polyscope::show()` blocks — wrap GUI runs in `timeout` when headless

## Sandbox rules (host policy; full ref: `/home/agent/SANDBOX-WORKFLOWS.md`)
- No root, read-only `/`; extra tooling lives in the overlay `/workspace/.sandbox/` (git-ignored via
  `.git/info/exclude`, except `.sandbox/SETUP_NOTES.md`, which is tracked on `dev`)
- Work strictly inside `/workspace`, scratch in `/tmp`; never push from the sandbox — the operator pushes
- Persistent cross-run changes need a proposal in `/workspace/.sandbox-requests/`

## Build environment in this sandbox (details: `.sandbox/SETUP_NOTES.md`)
Every build/run shell must first `source /workspace/.sandbox/env.sh` (puts the overlay toolchain on `PATH`/`CPATH`/`LIBRARY_PATH`):
- Linux: `cmake -S tutorial -B tutorial/build -G Ninja && cmake --build tutorial/build -j6` (`-j16` OOMs on 20 GiB RAM)
- Windows cross-build: `cmake -S tutorial -B tutorial/build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE=/workspace/.sandbox/mingw-toolchain.cmake && cmake --build tutorial/build-win -j6` → PE64 `.exe` in `tutorial/bin-windows/`
- GMP is auto-found by `tutorial/CMakeLists.txt` (honors `GMP_DIR`); here its headers/libs are in `.sandbox/rootfs` (Linux) or `.sandbox/prefixes/mingw` via the `mingw-shim` linker (Windows), not in `/usr`
- MinGW `x86_64-w64-mingw32-*` (gcc 13 posix) is in `.sandbox/rootfs/usr/bin`
- Polyscope GUI needs X11+GLX: binary-patched Xorg (dummy) on `:92`; run clients with `DISPLAY=:92 LIBGL_ALWAYS_SOFTWARE=1` + mesa DRI paths (SETUP_NOTES.md §6)
