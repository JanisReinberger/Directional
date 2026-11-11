# AGENTS.md — Directional in this sandbox

This file is auto-injected into agent sessions started in this repository. It records
the sandbox-specific rules and the build-environment setup so they are never
rediscovered the hard way.

## Sandbox rules (from the host policy, `/home/agent/.gemini/AGENTS.md`)

- The container has **no root and a read-only `/`** — `apt install` cannot work and
  nothing under `/usr` survives a restart. Only `/workspace` (synced to the
  `sandbox/<run-id>` git branch on exit) and `~/.vibe` (persistent volume) persist.
- Therefore all extra tooling lives in the **userland overlay `/workspace/.sandbox/`**
  (git-ignored via `.git/info/exclude`, so it never enters the branch).
- Work strictly inside `/workspace`; scratch goes to `/tmp`. Never push; commits
  publish automatically to the task branch. Persistent cross-run changes need a
  proposal in `/workspace/.sandbox-requests/`.
- Full reference: `/home/agent/SANDBOX-WORKFLOWS.md` and `/home/agent/.gemini/AGENTS.md`.

## Build environment (details: `.sandbox/SETUP_NOTES.md`)

Every shell that builds or runs the tutorials must first activate the overlay:

```bash
source /workspace/.sandbox/env.sh
```

This puts the overlay's toolchain on `PATH`/`CPATH`/`LIBRARY_PATH`:

- **Linux build:** `cmake -S tutorial -B tutorial/build -G Ninja && cmake --build tutorial/build -j6`
  → binaries in `tutorial/bin/` (use `-j6`; `-j16` OOMs on 20 GiB RAM).
- **Windows cross-build:** `cmake -S tutorial -B tutorial/build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE=/workspace/.sandbox/mingw-toolchain.cmake && cmake --build tutorial/build-win -j6`
  → PE64 `.exe` in `tutorial/bin-windows/` plus the three mingw runtime DLLs.
- **GMP:** found automatically by `tutorial/CMakeLists.txt` (honors `GMP_DIR`).
  Linux: `libgmp-dev` extracted into `.sandbox/rootfs`. Windows: GMP 6.3.0 built
  statically into `.sandbox/prefixes/mingw`, exposed through the `mingw-shim`
  linker script (link with `-lgmp` against the shim only). Do not look for GMP in
  `/usr` — the system only has the runtime `libgmp.so.10`, no headers.
- **MinGW:** `x86_64-w64-mingw32-*` (gcc 13 posix) in `.sandbox/rootfs/usr/bin`;
  the unversioned `x86_64-w64-mingw32-g++` symlink points at the `-posix` variant.
- **Polyscope GUI:** needs X11+GLX. A binary-patched Xorg (dummy driver) runs on
  `:92`; client env: `DISPLAY=:92 LIBGL_ALWAYS_SOFTWARE=1` + mesa DRI paths (see
  `SETUP_NOTES.md` §6). `polyscope::show()` blocks — wrap GUI runs in `timeout`.

A durable copy of this knowledge also ships as the Vibe plugin
`directional-build:build-environment` (`~/.vibe/plugins/directional-build/`).
