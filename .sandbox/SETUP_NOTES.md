# Directional — sandbox build & run notes (Ubuntu 24.04 container, WSL2, no root, read-only /)

## What was done
1. `git submodule update --init --recursive` — pulls `external/polyscope` (v2.5.0, all deps bundled as
   submodules: glfw, glad, imgui, glm, json, stb, happly, MarchingCubeCpp) and `external/googletest`.
   Eigen is vendored directly in `external/eigen`.
2. No root + read-only `/`: all packages fetched with `apt-get download` (apt state redirected to
   `/workspace/.sandbox/apt`) and extracted with `dpkg -x` into `/workspace/.sandbox/rootfs`.
   See `.sandbox/env.sh` for the resulting environment (PATH/LD_LIBRARY_PATH/CPATH/CMAKE_PREFIX_PATH...).
   Packages: X11/GL dev libs (glfw+glad need X11 + libGL), libgmp-dev, xserver-xorg-core + xorg modules,
   xkeyboard-config, xkbcomp, mesa (llvmpipe) + libglvnd, x11-apps/x11-utils, netpbm (xwd->png),
   mingw-w64 (gcc 13 posix), m4, wine64 (optional), etc. (~250 debs in `.sandbox/pkgs`).
3. GMP: Linux — `libgmp-dev` (libgmp + libgmpxx; `libgmp.so` replaced by a linker script so gmpxx links too).
   Windows — GMP 6.3.0 built from source with `x86_64-w64-mingw32-g++-posix --enable-static --enable-cxx`
   into `.sandbox/prefixes/mingw`; shim `.sandbox/prefixes/mingw-shim` exposes `libgmp.a` as a linker script
   pulling in `libgmpxx.a` (Directional's `ENumber_GMP.h` uses `mpz_class`/`mpq_class`).
4. Linux build: `cmake -S tutorial -B tutorial/build -G Ninja -DCMAKE_BUILD_TYPE=Release` with `GMP_DIR`
   set, then `ninja -j6` (-j16 OOMs: 23 GB RAM, Eigen-heavy TUs). Output: `tutorial/bin/*_bin` (25 binaries).
   CMake auto-detects GMP and defines `USE_GMP`/`USE_GMP_ENABLED` (used by `505_Meshing` via
   `include/directional/ENumber_GMP.h`).
5. Windows cross-build: toolchain file `.sandbox/mingw-toolchain.cmake` (mingw-w64 gcc 13 posix,
   `CMAKE_FIND_ROOT_PATH` = mingw sysroot + GMP shim), `cmake -S tutorial -B tutorial/build-win
   -DCMAKE_TOOLCHAIN_FILE=... -G Ninja`, `ninja -j6`. Output: `tutorial/bin-windows/*.exe` (25 PE64)
   + runtime DLLs `libstdc++-6.dll`, `libgcc_s_seh-1.dll`, `libwinpthread-1.dll` copied alongside.
   `505_Meshing_bin.exe` has GMP statically linked (205 `__gmp*` symbols).
6. GUI: Polyscope v2.5.0 `openGL3_glfw` backend needs X11+GLX. Plain Xvfb lacks loadable GLX here, so the
   full `Xorg` binary (dummy video driver) was binary-patched (same-length string swaps) to find
   xkbcomp/modules/DRI-driver under /tmp, and run as:
   `/tmp/xkb/Xorg :92 -config /tmp/xorg-dummy.conf -noreset -ac -xkbdir <rootfs>/usr/share/X11/xkb`
   Client env: `DISPLAY=:92 LIBGL_ALWAYS_SOFTWARE=1 LIBGL_DRIVERS_PATH=<rootfs>/usr/lib/x86_64-linux-gnu/dri
   __GLX_VENDOR_LIBRARY_NAME=mesa` → llvmpipe OpenGL 4.5.
   All 25 Linux binaries launch and compute (see `.sandbox/smoke.log`); screenshot of 101 saved at
   `.sandbox/shot101.png` (Polyscope window with mesh + glyphs + singularities). 505 runs the GMP mesher.
   Polyscope also has an EGL headless backend (works via `eglinfo -p surfaceless`), but stock tutorials call
   `polyscope::init()` which requires a display unless `options::allowHeadlessBackends=true` is set in code.

## Host-side (Windows 10 + WSL2)
- **WSLg IS available on Windows 10** (backported; verified 2026-11-11 by running a tutorial binary from
  the host WSL2 shell on Win10 — the Polyscope window appears natively). On the host WSL shell just run
  the Linux binaries with the WSLg display (`DISPLAY` is set automatically, usually `:0`):
  `source /workspace/.sandbox/env.sh && /workspace/tutorial/bin/101_GlyphRendering_bin`
  (binaries link X11 dynamically → keep the env.sh LD_LIBRARY_PATH, or install the libs on a normal distro).
  NOTE: `/mnt/wslg` is NOT mounted inside this sandbox container, so *inside the container* the patched
  Xorg on `:92` (auto-started by env.sh) remains the display. Only on the host's own WSL shell use WSLg.
  If WSLg is unavailable and you want interactive GUI from the container, install an X server on Windows
  (VcXsrv or Xming, allow TCP/-ac), then in the container: `export DISPLAY=host.docker.internal:0`
  (host.docker.internal resolves to 172.24.112.1 here).
- Windows binaries: copy `tutorial/bin-windows/` to the Windows machine and run the .exe (needs the 3
  mingw runtime DLLs sitting next to them; system DLLs GDI32/USER32/KERNEL32/msvcrt/SHELL32 are built-in).
  Alternatively rebuild on Windows with MSVC + cmake-gui (repo docs recommend this) — GMP via vcpkg
  (`vcpkg install gmp`) or the prebuilt in `.sandbox/prefixes/mingw` won't mix with MSVC; use vcpkg.

## Impersonalities / caveats
- No root, read-only `/`, no user namespaces: apt/wine/X servers cannot be installed system-wide; everything
  lives in `/workspace/.sandbox/rootfs` and binaries were patched to relocate their hardcoded paths.
- Running the .exe under Wine inside this sandbox was attempted but Wine's own hardcoded `/usr/...` paths
  block it without root — not needed on a real Windows host.
- Xvfb alone has no GLX (module path baked in); Xorg+dummy driver was used instead.
- `polyscope::show()` blocks until the window is closed — expected; use `timeout` for automated runs.
- Build dirs `tutorial/build`, `tutorial/build-win`, `tutorial/bin`, `tutorial/bin-windows` are excluded
  via `.git/info/exclude` (repo `.gitignore` untouched).

## Persistence across sandbox restarts (added 2026-11-11)
- What survives: **`/workspace`** (real disk) and **`~/.vibe`** — so everything in
  `.sandbox/` (rootfs, pkgs, prefixes, toolchain file, patched Xorg in `.sandbox/xorg/`,
  `env.sh`, `start-xorg.sh`) persists. What does NOT survive: **`/tmp`** (tmpfs, wiped on
  every container restart) — the X socket, the patched-binary symlinks, and any running
  Xorg process.
- Fix: `source /workspace/.sandbox/env.sh` now **auto-starts the headless Xorg on :92**
  (DISPLAY defaults to :92). `env.sh` calls `start-xorg.sh`, which recreates the four
  `/tmp` symlinks (`/tmp/xkb -> .sandbox/xorg`, `/tmp/bin/xkbcomp`,
  `/tmp/lib/x86_64-linux-gnu`, `/tmp/share`) and launches the pre-patched
  `.sandbox/xorg/Xorg` with `setsid` (survives shell exit). Idempotent + lock-guarded.
  If `.sandbox/xorg/` is ever missing, `start-xorg.sh` re-derives the patched binaries
  from `rootfs/` with the same-length string swaps (procedure embedded in the script).
- Manual: `/workspace/.sandbox/start-xorg.sh` (prints status + GLX check).
- Client env for GUI runs: `DISPLAY=:92 LIBGL_ALWAYS_SOFTWARE=1
  LIBGL_DRIVERS_PATH=$ROOTFS/usr/lib/x86_64-linux-gnu/dri __GLX_VENDOR_LIBRARY_NAME=mesa`,
  wrap in `timeout` (polyscope::show blocks).
- Verified after a simulated restart (Xorg killed, /tmp wiped): sourcing env.sh alone
  brings back GLX swrast (`openGL 4.5 Mesa llvmpipe`) and all 25 `tutorial/bin/*_bin`
  compute correctly; both ninja build trees report "no work to do".

## Relocatable data/output paths (2026-11-11)
`tutorial/CMakeLists.txt` now bakes `TUTORIAL_DATA_PATH="../data"` and
`TUTORIAL_OUTPUT_PATH="../output"` (relative), and force-includes the new
`tutorial/tutorial_paths.h` into every tutorial target, which chdir()s to the
executable's own directory before main() (Linux: /proc/self/exe, Windows:
GetModuleFileNameA; MSVC gets /FI). Result: binaries find `data/` next to
`bin`/`bin-windows` regardless of CWD, on both Linux and Windows exes.
Verified: all 25 Linux binaries run from CWD=/ (OFF/OBJ/rawfield all load);
exes in bin-windows contain "../data" and no absolute path. If you copy
bin-windows elsewhere, keep a `data/` (and `output/`) folder as a sibling of
the binary folder. Absolute -DTUTORIAL_DATA_PATH=... overrides still work.

## env.sh on the host WSL vs in-container (2026-11-11)
- env.sh now derives SANDBOX from its own path (BASH_SOURCE), not a hardcoded
  /workspace, and only redirects HOME when running as the container user
  (HOME=/home/agent). So it is safe to source in the HOST WSL shell too, where the
  same workspace lives at ~/.pi-sandbox/runs/<run-id>/workspace/.sandbox.
- Binaries run WITHOUT env.sh on the host WSL: the host distro provides the X11/GL
  runtime libs and WSLg provides DISPLAY (:0), and libgmp.so.10 exists system-wide.
  (Sourcing env.sh there previously printed "Failed to create /workspace for shader
  cache (Permission denied)" because HOME was redirected to the nonexistent
  /workspace/.sandbox/home — fixed by the two changes above.)
- IN THE CONTAINER env.sh (or at least its LD_LIBRARY_PATH) IS still required:
  the base image has no libX11 (`ldd bin/101_GlyphRendering_bin` -> "libX11.so.6 =>
  not found" without it); only libgmp.so.10 is present system-wide.
- WSLg note: /mnt/wslg is NOT mounted inside the container, so in-container GUI uses
  the auto-started patched Xorg on :92 (env.sh). Mounting /mnt/wslg would give the
  container native WSLg GUI (see .sandbox-requests/wslg-mount.md) but requires a
  host-side bind mount at container start (no root inside) and is opt-in for safety.

## WSLg inside the container — VERIFIED working (2026-11-11, after PI_SANDBOX_MOUNT_WSLG=1)
- The launcher now bind-mounts /mnt/wslg + /usr/lib/wsl/lib and presets
  `DISPLAY=:0`, `WAYLAND_DISPLAY=/mnt/wslg/wayland-0` (actual socket: /mnt/wslg/runtime-dir/wayland-0),
  `XDG_RUNTIME_DIR=/run/user/1000`, and symlinks /tmp/.X11-unix -> /mnt/wslg/.X11-unix.
- Usage in-container: `source /workspace/.sandbox/env.sh` (gives the X11 client libs from
  rootfs; the :92 auto-start is correctly SKIPPED because DISPLAY is already :0), then just
  run e.g. `timeout 60 /workspace/tutorial/bin/101_GlyphRendering_bin` — a real Polyscope
  window appears on the Windows desktop (verified: xwininfo shows the GLFW window on :0,
  vendor "Microsoft Corporation", screen 3440x1440; screenshot captured via
  `xwd -id <winid> -display :0`). The :92 dummy Xorg remains as fallback for headless/CI
  (unset DISPLAY to get it).
- Rendering is llvmpipe over Xwayland (software) unless the WSLg d3d12 GL stack is put
  first in LD_LIBRARY_PATH (/mnt/wslg/distro/usr/lib/x86_64-linux-gnu + its dri dir).
- SECURITY: mounting all of /mnt/wslg also exposes /mnt/wslg/distro/home/<user> = the
  HOST home directory (readable). Prefer mounting only /mnt/wslg/.X11-unix,
  /mnt/wslg/runtime-dir and the Pulse sockets — see .sandbox-requests/wslg-mount.md.
