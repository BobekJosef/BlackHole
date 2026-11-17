# BlackHole

Interactive, GPU-accelerated simulation of a rotating (Kerr) black hole, written in C++23 and OpenGL 4.3.
The goal is a physically based image — light bent along null geodesics of the Kerr metric, a thin
accretion disk with relativistic Doppler and gravitational redshift — that you can fly around in real time.

> **Status: Stage 1 done.** The GPU pipeline (compute tracing, bloom, tone mapping, timing), camera,
> control panel and closed-form Kerr physics work; the image is still a flat-space preview (no lensing).
> See [TODO.md](TODO.md) for the roadmap.

## Features

- Runs on the GPU: a compute shader traces one ray per pixel into an HDR image, followed by bloom and
  tone mapping (AgX or ACES) passes
- Orbit camera (drag to rotate, scroll to zoom), adjustable field of view, exposure in EV
- Dear ImGui panel: spin `a/M`, prograde/retrograde disk, disk on/off and size, star background on/off,
  render scale (25–100 % of the window resolution), tone mapper, bloom
- Live readouts of the horizon, ergosphere, photon orbit and ISCO radii for the chosen spin
- GPU time per pass (trace, bloom, present, UI) in the panel
- Shader hot reload: edit anything in `shaders/` while the app runs; it recompiles on save, and errors
  show up in the panel with file and line

Planned: Kerr geodesic ray tracing, Novikov–Thorne disk with Doppler beaming and redshift, HDR image and
cubemap backgrounds, adaptive resolution. Details in [TODO.md](TODO.md).

## Requirements

- A C++23 compiler: GCC ≥ 13, Clang ≥ 17, or MSVC 19.37+ (Visual Studio 2022 17.7)
- CMake ≥ 3.25 and Git (dependencies are downloaded at configure time)
- A GPU and driver with OpenGL 4.3 core (any GPU since ~2012). macOS is not supported: Apple stops at OpenGL 4.1.
- Linux: X11 development headers for GLFW

  ```sh
  sudo apt install build-essential cmake git libgl-dev \
       libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev   # Debian/Ubuntu
  sudo dnf install gcc-c++ cmake git mesa-libGL-devel \
       libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel   # Fedora
  ```

Optional: `glslang-tools` (Debian/Ubuntu) adds offline validation of every shader to `ctest`.

GLFW 3.4, GLM 1.0.1 and Dear ImGui 1.91.9b are fetched automatically (an installed GLFW or GLM, e.g. from
vcpkg, is used instead when CMake finds one). The GLAD loader is vendored in `external/glad`.

## Build and run

All commands run from the repository root. The first configure downloads the dependencies (about 20 s);
later ones reuse them.

```sh
cmake --preset release            # configure into build/release
cmake --build --preset release -j # compile (-j: parallel build)
./build/release/src/blackhole     # run
```

For development, use the Debug build: warnings are errors, and OpenGL driver messages are printed to the
terminal.

```sh
cmake --preset debug
cmake --build --preset debug -j
ctest --preset debug              # unit tests
./build/debug/src/blackhole
```

| Preset           | Build type     | Use it for                                          |
|------------------|----------------|-----------------------------------------------------|
| `release`        | Release        | running the simulation, measuring performance       |
| `debug`          | Debug          | development (warnings as errors, GL debug output)   |
| `asan`           | Debug          | hunting memory bugs (AddressSanitizer + UBSan)      |
| `relwithdebinfo` | RelWithDebInfo | profiling with symbols                              |

Each preset has its own directory under `build/`, so they can coexist. After editing a `CMakeLists.txt`,
just build again; CMake re-configures itself. Shaders are read from `shaders/` in the source tree at run
time, so editing one needs no rebuild: the running app recompiles it on save (or press **R**).
Dear ImGui stores the panel layout in `imgui.ini` in the working directory (git-ignored).

IDEs: CLion, VS Code (CMake Tools) and Visual Studio pick up `CMakePresets.json` directly; select a preset
and build the `blackhole` target. Without presets (e.g. older tools):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/src/blackhole
```

Troubleshooting:
- *configure fails in GLFW with a missing `X11/extensions/...` header*: install the Linux packages above,
  then delete `build/<preset>` and configure again.
- *"cannot create a window with an OpenGL 4.3 core context"*: the GPU driver is too old or software
  rendering is active; check with `glxinfo -B` (Linux) that a hardware renderer reports 4.3 or newer.
- On a laptop with two GPUs, run on the discrete one, e.g. `DRI_PRIME=1 ./build/release/src/blackhole`
  (Mesa) or `__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia ./build/release/src/blackhole`.

## Controls

| Input              | Action                       |
|--------------------|------------------------------|
| Left mouse drag    | Orbit around the black hole  |
| Scroll             | Zoom in / out                |
| R                  | Recompile shaders now        |
| Esc                | Quit                         |

## Project layout

```
src/physics/   closed-form Kerr formulas (no OpenGL; unit-tested)
src/gfx/       window, OpenGL object wrappers, shader programs
src/render/    the black hole renderer
src/scene/     camera and user settings
src/ui/        Dear ImGui layer and control panel
src/app/       application and frame loop
shaders/       GLSL (shared headers in shaders/common)
tests/         unit tests
external/glad/ generated OpenGL loader
```

Units are geometric (`G = c = M = 1`): distances are in multiples of `GM/c²`.
