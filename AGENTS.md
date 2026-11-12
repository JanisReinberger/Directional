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

## Git branching rules (follow exactly — do not improvise)

Two remotes:
- `upstream` = the original library (`avaxman/Directional`). PRs target its `master`. Never push to it.
- `origin` = your own fork. **Agents NEVER push. The operator pushes.**

Three branches, three jobs:

| Branch | Job | Hard rule |
|---|---|---|
| `master` | exact mirror of `upstream/master` | NEVER commit here. Update only: `git fetch upstream && git checkout master && git merge --ff-only upstream/master` |
| `dev` | your working branch: `master` + env files (AGENTS.md, `.sandbox/SETUP_NOTES.md`, `.gitignore`) + all features, joined by merge commits. Agent sessions run here. | never rebase / reset / rewrite it |
| `feature/<name>` | clean PR branch = `master` + only that feature's commits. Exists only to be pushed and PR'd upstream. | create from `master` (or from a parent `feature/*` if it depends on an unmerged PR) |

### 1. Normal work (most cases): commit directly on `dev`
```bash
git checkout dev
# ...edit files...
git add -A
git commit -m "short description of the change"
```
No new branch needed for personal work.

### 2. Finish a feature branch: merge it into `dev` (the merge commit is required)
```bash
git checkout dev
git merge feature/<name> -m "Merge branch 'feature/<name>' into dev"
```
The merge commit is what records in the graph that the feature landed in `dev`.

### 3. Offer a feature upstream as a PR
```bash
# a) independent feature:
git checkout -b feature/<name> master
# b) feature that depends on an open, unmerged PR:
git checkout -b feature/<name> feature/<parent>
# copy the commits over from dev:
git cherry-pick <commit-shas>
# GUARDRAIL — output must list ONLY files under include/ or tutorial/ (plus .gitignore):
git diff master...feature/<name> --stat
# if it lists AGENTS.md / .sandbox/... → branch is dirty, start over from master
# (.gitignore is allowed on a feature branch: ignores for generated tutorial outputs and
#  the local ImGui layout state belong with the feature that produces them)
# operator then pushes and opens the PR (agents never push):
git push -u origin feature/<name>    # PR on GitHub, base = upstream/master
```
Then merge the branch into `dev` (rule 2). Example already in this repo: `feature/rounding`
was merged into `dev` in merge commit `2212846`.

### 4. Fix an existing feature
- Feature only exists on `dev` → just commit the fix on `dev` (rule 1).
- Feature has an open PR branch → commit the fix on `feature/<name>`; operator force-pushes it
  (`git push -f origin feature/<name>`); then update `dev`: `git checkout dev && git merge feature/<name>`.
- Feature already merged upstream → new branch from `master`, fix, new PR (rule 3).

### 5. Keep up with upstream (merge-based — NEVER rebase `dev`)
```bash
git fetch upstream
git checkout master && git merge --ff-only upstream/master
git checkout dev && git merge upstream/master    # creates "Merge upstream/master into dev"
```
If a stacked PR's parent merged upstream, shrink the child branch:
`git rebase --onto master <parent-pr-tip> feature/<child>` (drops commits already upstream).

### NEVER
1. NEVER commit on `master`.
2. NEVER push from an agent sandbox.
3. NEVER run these on `dev`, `master`, or a `feature/*` branch: `git reset --hard`, `git rebase`,
   `git branch -f`, `git filter-branch`, `git filter-repo`, `git commit --amend` on pushed commits.
   If you think you need one: STOP and ask the operator first.
4. NEVER force-push anything except your own open `feature/<name>` after review fixes.
5. NEVER merge `master` into a `feature/*` branch (it would dirty the PR diff).

### Housekeeping
- `sandbox/*` branches and `refs/pi-publish/*` refs are agent-harness artifacts — ignore them
  unless the operator says otherwise.

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
