# BlackHole

Interactive, GPU-accelerated simulation of a rotating (Kerr) black hole, written in C++23 and OpenGL 4.3.
The goal is a physically based image — light bent along null geodesics of the Kerr metric, a thin
accretion disk with relativistic Doppler and gravitational redshift — that you can fly around in real time.

> **Status: Stage 5 done.** Light follows null geodesics of the Kerr metric, traced on the GPU; the
> accretion disk is a Novikov–Thorne thin disk whose black-body light is Doppler-shifted, beamed and
> gravitationally redshifted; the background is a lensed star field, a coordinate grid or any sky map.
> Next: interactivity and performance (Stage 6). See [TODO.md](TODO.md).

## Features

- General-relativistic ray tracing on the GPU: one null geodesic of the Kerr metric per pixel (RK4 in
  Kerr–Schild coordinates), seen by a zero-angular-momentum observer: lensed disk, D-shaped shadow,
  photon rings, lensed stars
- Novikov–Thorne thin accretion disk from the innermost stable circular orbit (ISCO) outwards: Page–Thorne
  temperature profile, black-body colours from the CIE colour matching functions, relativistic Doppler
  shift and beaming, gravitational redshift, limb darkening (each can be switched off to see its effect);
  it extends far out, fades as it cools, and thins out into transparency at its outer edge. Optional
  animated turbulence (a statistical model of the magnetorotational instability that keeps the mean
  Novikov–Thorne flux), sheared by the Keplerian flow
- Backgrounds: point-source stars that brighten with the lensing magnification (both images of a star
  behind the hole show), a coordinate grid that makes the lensing map readable, or your own equirectangular
  or cube-map sky (`.hdr`, PNG, JPEG, …), all filtered with each pixel's footprint on the sky so nothing
  aliases next to the shadow. The camera's own gravitational blueshift of the sky is included.
- Validation tools: debug views (step count, integration error, ray fate, redshift, magnification) and an
  overlay of the analytic shadow edge
- HDR pipeline: bloom and tone mapping (AgX or ACES), optional auto exposure; a still view refines into
  an anti-aliased image (jittered frames averaged), then the GPU idles
- Observers: a static camera (the zero-angular-momentum observer), one orbiting at the Keplerian speed, or
  one falling freely into the hole (dropped, or launched sideways into a plunging spiral), each seeing the
  aberration and Doppler shift its motion causes; look around in any of them
- Orbit camera (drag to rotate, scroll to zoom), adjustable field of view, exposure in EV
- Dear ImGui panel: spin `a/M`, prograde/retrograde disk, disk on/off and size, background choice,
  brightness and orientation, integrator step size and step limit, render scale (25–100 % of the window resolution), tone mapper, bloom
- Live readouts of the horizon, ergosphere, photon orbit and ISCO radii for the chosen spin
- GPU time per pass (trace, bloom, present, UI) in the panel
- Shader hot reload: edit anything in `shaders/` while the app runs; it recompiles on save, and errors
  show up in the panel with file and line

Planned: adaptive resolution, temporal anti-aliasing, moving cameras, a real star catalogue. Details in
[TODO.md](TODO.md).

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

GLFW 3.5.1, GLM 1.0.3, Dear ImGui 1.93.0, stb_image and (for the tests) doctest 2.5.3 are fetched automatically (an installed GLFW or GLM, e.g. from
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
ctest --preset debug              # unit tests (doctest; one CTest entry per test case)
./build/debug/src/blackhole
```

| Preset           | Build type     | Use it for                                          |
|------------------|----------------|-----------------------------------------------------|
| `release`        | Release        | running the simulation, measuring performance       |
| `debug`          | Debug          | development (warnings as errors, GL debug output)   |
| `asan`           | Debug          | hunting memory bugs (AddressSanitizer + UBSan)      |
| `relwithdebinfo` | RelWithDebInfo | profiling with symbols                              |

Quality checks:

```sh
./build/release/src/blackhole --validate   # GPU tracer vs the double-precision CPU reference (needs a GPU)
scripts/lint.sh                            # clang-format + clang-tidy (--fix reformats first)
LSAN_OPTIONS=suppressions=$PWD/scripts/lsan.supp ./build/asan/src/blackhole   # sanitizers, GPU-driver leaks muted
```

`cmake --preset debug -DBLACKHOLE_GPU_TESTS=ON` adds the GPU validation to `ctest`. CI
(`.github/workflows/ci.yml`) builds with GCC, Clang (with sanitizers) and MSVC, and runs the unit tests and
the linters; it has no GPU.

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

## Sky maps

The *Image map* background needs a map in `assets/` (git-ignored). Fetch the default one with

```sh
scripts/download_sky_maps.sh   # ESO Milky Way panorama, 8 MB (CC BY 4.0, ESO/S. Brunier)
```

or drop in your own: an equirectangular image (width = 2 × height; Radiance `.hdr` keeps the stars'
brightness ratios) or a folder of six cube faces (`px nx py ny pz nz`). Pick it in the panel (*Rescan*
after adding files), or type any path into the *Path* field. Details in [assets/README.md](assets/README.md).

## Presets and screenshots

The panel's *Presets* section loads a scene and camera from `presets/` (edge-on, face-on, lensing grid,
Milky Way, orbiting, plunge) and saves the current one under a name. A preset is a short text file:

```ini
# keys left out take their default values
spin = 0.9
disk.peak_temperature = 6500
camera.polar = 84
camera.distance = 26
```

**F12** (or *Save PNG* in the panel) saves a screenshot without the panel into `screenshots/`, rendered
again at full quality: at the window's size, or at a fixed size up to 7680 × 4320.

## Controls

| Input              | Action                       |
|--------------------|------------------------------|
| Left drag          | Orbit around the black hole  |
| Right drag         | Look around (every camera mode) |
| Right double click, C | Look back at the hole      |
| Wheel click        | Orbit on/off without holding a button: slower, cursor hidden and not stopped by the screen edges (a left click or Esc also ends it) |
| Scroll             | Zoom in / out                |
| F11                | Fullscreen on/off            |
| F12                | Screenshot (PNG)             |
| R                  | Recompile shaders now        |
| Ctrl+C (terminal)  | Close cleanly                |
| Esc                | End wheel-click orbiting, else leave fullscreen, else quit |

## Project layout

```
src/physics/   physics in double precision: Kerr closed forms, metric, geodesics, disk, black body (unit-tested)
src/assets/    sky map loading (unit-tested)
src/gfx/       window, OpenGL object wrappers, shader programs
src/render/    the black hole renderer
src/scene/     camera, user settings, presets
src/ui/        Dear ImGui layer and control panel
src/app/       application and frame loop
shaders/       GLSL (shared headers in shaders/common)
tests/         unit tests
assets/        sky maps (downloaded, not committed)
presets/       scene presets (key = value text)
screenshots/   F12 output (not committed)
scripts/       download_sky_maps.sh
external/glad/ generated OpenGL loader
```

Units are geometric (`G = c = M = 1`): distances are in multiples of `GM/c²`.
