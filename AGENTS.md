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

## Conventions
- Header-only library: each header generally provides one function; C++20 (`CMAKE_CXX_STANDARD 20`)
- Tutorial chapters named `NNN_Topic`, one executable per chapter named `NNN_Topic_bin`
- Polyscope `polyscope::show()` blocks — wrap GUI runs in `timeout` when headless

## Sandbox rules (host policy; full ref: `/home/agent/SANDBOX-WORKFLOWS.md`)
- No root, read-only `/`; extra tooling lives in the git-ignored overlay `/workspace/.sandbox/`
- Work strictly inside `/workspace`, scratch in `/tmp`; never push — commits publish automatically to the task branch
- Persistent cross-run changes need a proposal in `/workspace/.sandbox-requests/`

## Build environment in this sandbox (details: `.sandbox/SETUP_NOTES.md`)
Every build/run shell must first `source /workspace/.sandbox/env.sh` (puts the overlay toolchain on `PATH`/`CPATH`/`LIBRARY_PATH`):
- Linux: `cmake -S tutorial -B tutorial/build -G Ninja && cmake --build tutorial/build -j6` (`-j16` OOMs on 20 GiB RAM)
- Windows cross-build: `cmake -S tutorial -B tutorial/build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE=/workspace/.sandbox/mingw-toolchain.cmake && cmake --build tutorial/build-win -j6` → PE64 `.exe` in `tutorial/bin-windows/`
- GMP is auto-found by `tutorial/CMakeLists.txt` (honors `GMP_DIR`); here its headers/libs are in `.sandbox/rootfs` (Linux) or `.sandbox/prefixes/mingw` via the `mingw-shim` linker (Windows), not in `/usr`
- MinGW `x86_64-w64-mingw32-*` (gcc 13 posix) is in `.sandbox/rootfs/usr/bin`
- Polyscope GUI needs X11+GLX: binary-patched Xorg (dummy) on `:92`; run clients with `DISPLAY=:92 LIBGL_ALWAYS_SOFTWARE=1` + mesa DRI paths (SETUP_NOTES.md §6)
