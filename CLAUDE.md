# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

BlackHole is an interactive, physically based renderer of a rotating (Kerr) black hole with an optional
accretion disk and a configurable background. The rendering runs on the GPU (OpenGL 4.3 core, GLSL);
the C++ side owns the window, input, UI and the closed-form physics. `TODO.md` is the roadmap; Stages 0–5
are done: rays follow Kerr null geodesics (`trace.comp` + `common/kerr.glsl`); the disk is a
thin opaque Novikov–Thorne disk from the ISCO whose black-body light is shifted by `g` (Doppler,
gravitational, frame dragging; `common/disk.glsl`); escaped rays see stars, a grid or a sky map, filtered
with a ray differential shared through the work group (`common/sky.glsl`). Next: Stage 6 (interactivity
and performance).

A frame is three GPU passes (`BlackHoleRenderer`): **trace** (compute, one ray per pixel at
`renderScale` × window size → HDR radiance image) → **bloom** (compute down/up-sampling chain) →
**present** (fragment: up-scale, bloom mix, exposure, tone mapping, sRGB). ImGui draws on top.

Stack: C++23, CMake ≥ 3.25, OpenGL 4.3 core, GLFW 3.4, GLAD 2 (vendored), GLM 1.0, Dear ImGui 1.91,
stb_image.
macOS is unsupported (OpenGL stops at 4.1 there, and compute shaders need 4.3).

## Build, run, test

```sh
cmake --preset debug          # Debug, warnings are errors; binary dir build/debug
cmake --build --preset debug -j
ctest --preset debug
./build/debug/src/blackhole

cmake --preset release && cmake --build --preset release -j   # for anything performance-related
cmake --preset asan    && cmake --build --preset asan -j      # ASan + UBSan
```

Run commands from the repository root. Always pass `-j`: the default generator is Make, which builds
serially otherwise. Running the app opens a window on the user's display: keep such runs short, start the
binary itself in the background (`binary > log 2>&1 & pid=$!` — never `cd dir && binary &`, whose `$!` is
a wrapper shell, so `kill` leaves the window open), kill it, and confirm with `pgrep -x blackhole`.

- Dependencies come from `cmake/Dependencies.cmake`: GLFW and GLM through `find_package` when an installed
  package (system or vcpkg) matches, otherwise FetchContent; ImGui and stb_image (pinned commit; its
  implementation unit is generated there, so `STB_IMAGE_IMPLEMENTATION` stays a compile definition)
  always through FetchContent; GLAD is generated code committed in `external/glad` (regeneration command
  in its README).
- Sky maps live in `assets/` (`config::assetDir`), git-ignored except its README; `scripts/download_sky_maps.sh`
  fetches the default one. Never commit images.
- Linux needs the X11 headers for GLFW: `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev`,
  and `libgl-dev`. GLFW's Wayland backend is off by default (`-DGLFW_BUILD_WAYLAND=ON` enables it).
- Shaders are read at run time from the source tree (`config::shaderDir`, generated into
  `build/*/src/generated/blackhole/config.hpp`), so a shader edit needs no rebuild: the app recompiles
  within a second of a save (or on **R**). A failed recompile keeps the previous programs and shows the
  log, mapped to `file:line`, at the top of the panel.
- With `glslang-tools` installed, `ctest` also validates every shader offline (tests `glsl:<file>`). By hand:
  `glslangValidator -G --aml -Ishaders -o /dev/null shaders/trace.comp`. `-G` (OpenGL SPIR-V) is required:
  in plain GLSL mode glslang does not resolve `#include`.
- Debug builds create a debug GL context; `gl::enableDebugOutput` prints every driver message above
  "notification" to stderr. Any such message is a bug.

## Layout

```
src/physics/   bh_physics (no OpenGL, no GLM, double precision, unit-tested):
               Kerr (closed forms, shadow outline), KerrSchild (metric, ZAMO tetrad),
               Geodesic (Hamiltonian equations, RK4 tracer: the CPU reference for the shaders),
               Disk (Page-Thorne flux, temperature profile, redshift g), Blackbody (Planck x CIE -> sRGB),
               Observer (Keplerian and free-falling cameras: timelike geodesics in proper time)
src/assets/    bh_assets (no OpenGL, unit-tested): SkyImage (stb_image decoding of equirectangular and
               cube maps; runs on a worker thread, Application::updateSkyImage)
src/gfx/       thin OpenGL/GLFW wrappers: Window, GlObject (RAII names), ShaderProgram, GlDebug, GpuTimer;
               ShaderPreprocessor (GLSL #include; no GL, so its own bh_shadertools library, unit-tested)
src/render/    BlackHoleRenderer (the passes), FrameParams (CPU-side frame description),
               FrameUniforms (std140 mirror of shaders/common/frame.glsl), GpuPass (timed sections)
src/scene/     OrbitCamera, SceneSettings (what the UI edits)
src/ui/        ImGuiLayer (context + backends), ControlPanel (a class: it keeps the path text field)
src/app/       Application: owns everything, runs the frame loop
shaders/       main shaders: trace.comp, bloom_*.comp, fullscreen.vert + present.frag
shaders/common/ shared GLSL headers: frame (UBO), kerr (geodesics), disk (redshift, emission),
               color (tone mapping, sRGB), sky (stars, grid, maps; footprint filtering), random
tests/         CTest executables (bh_physics, bh_shadertools, bh_assets; no GL context in tests) + GLSL validation
assets/        sky maps, git-ignored (README.md lists sources and licences)
scripts/       download_sky_maps.sh
external/glad/ vendored loader
```

Keep the physics library free of OpenGL and GLM so it can serve as the CPU double-precision reference
for the GPU code (TODO.md, Stage 7). The renderer takes a `FrameParams`, never `SceneSettings`: UI state
is converted to GPU quantities in one place (`Application::frameParams`).

## Physics conventions

- **Geometric units** `G = c = M = 1`: lengths in `GM/c²`, times in `GM/c³`. Shaders, C++ and the UI all
  use them; label UI values with "M".
- Metric signature `(-, +, +, +)`. Spin `a = J/M` with `0 ≤ a ≤ 1` (the UI stops at Thorne's 0.998); the
  sense of the disk is the separate `kerr::Orbit`, not a negative `a`.
- World frame: spin axis `+z`, equatorial plane `z = 0`, the hole rotates counterclockwise seen from `+z`.
  Boyer–Lindquist `(t, r, θ, φ)` for formulas and readouts; **outgoing** Kerr–Schild Cartesian coordinates
  for everything traced (camera position, rays, disk hits). In them `z = r cos θ` exactly, and on the
  equator `r = √(x² + y² − a²)`. Outgoing because rays run backwards in time and captured ones approach
  the past horizon, where the ingoing form is singular (see `KerrSchild.hpp`).
- Camera rays are **past-directed**: `p = −u + n` for a pixel looking along `n` in the camera frame `u`.
  Never `u + n`: that is only equivalent in static spacetimes and mirrors the spin in Kerr.
- The camera is any observer: `kerr::observerTetrad` around its 4-velocity (the ZAMO, a Keplerian
  orbiter, a free faller). Nothing downstream may assume the ZAMO: shifts are `1/(p·u_source)` with
  `p·u_camera = 1`, which holds for every observer. Analytic overlays that do assume it (the Bardeen
  shadow edge) are hidden for moving cameras.
- Light from the disk: a black body at `T` seen with shift `g` is a black body at `gT` (`I_ν/ν³` invariant),
  so radiance is one look-up of `gT` in the Planck table: never multiply by an extra `g⁴`. Ray momenta are
  normalised so the camera ZAMO measures frequency 1, which is what makes `g = 1/(p·u_emitter)`.
- The GLSL physics is a float copy of `src/physics/Geodesic.cpp` and `Disk.cpp`. Change the C++ first, prove it with a
  test in `tests/test_geodesic.cpp`, then port; keep the two algebraically identical (the GLSL may
  regroup terms for speed, e.g. reciprocals). Check the result in the app with the *Constraint* debug
  view and the *Analytic shadow edge* overlay.
- Every formula cites its source next to it (paper and equation number). Prefer a closed form with a test
  against the known limits (`a = 0`, `a = 1`, prograde vs retrograde) over a fitted number.
- Light from the sky reaches the camera shifted by `g = 1/p_t` (`kerr::skyRedshift`). Black-body sources
  (the procedural stars) take it exactly through the Planck table; RGB images and the grid only get the
  bolometric `g⁴`.
- Sky lookups never use screen-space derivatives: lensing makes them meaningless. A pixel's footprint on the
  sky is the change of the escape direction to its neighbours (`sEscape` in `trace.comp`); filter with it
  (`textureGrad`, analytic box filters) and take the magnification `μ` from it. Point sources get radiance
  `F / (sky solid angle of the pixel)`, which is flux-conserving at any magnification.
- Orientation of sky maps: equirectangular with z up, φ = 0 at the image centre, unmirrored from inside;
  cube maps through the reflection `xzy` (OpenGL's cube convention is left-handed). Check changes with a
  labelled test map, not by eye on a star field.
- Disk turbulence (`diskTurbulence` in `common/disk.glsl`) is a statistical model: it may change the look but
  must keep the mean flux equal to Page–Thorne (lognormal with the `−σ²Var/2` correction) and must fade
  below the pixel footprint. `uTime` is coordinate time in M (the animation clock), not seconds. Anything
  animated must look the same after hours as after seconds: no phase like `t / T(r)` whose spatial
  gradient grows with `t`; test by starting the clock late (e.g. 20000 M).
- Anything not physical (artistic exposure, fitted colours, procedural star positions) is marked as such in a
  comment, and the physically meaningful path stays the default.

## GPU and GLSL conventions

- `#version 430 core` in every main shader, followed by `#extension GL_GOOGLE_include_directive : enable`
  (for glslang; `ShaderPreprocessor` blanks it). Shared code goes in `shaders/common/*.glsl` with an
  `#ifndef NAME_GLSL` guard (the preprocessor does not deduplicate) and is pulled in with
  `#include "common/x.glsl"`, resolved relative to the including file.
- Uniforms `uCamelCase`, stage inputs/outputs `vCamelCase`, constants `UPPER_SNAKE_CASE`; functions and
  locals `camelCase` as in C++. GLSL `#define` only for include guards.
- Per-frame scene data goes in the `Frame` UBO (binding 0); a new field changes `common/frame.glsl` and
  `FrameUniforms.hpp` together, with a new `static_assert`. Pass-specific knobs stay plain uniforms.
- Compute shaders use 8×8 work groups (`workGroupSize` in the renderer) and return early outside the image,
  except `trace.comp`, whose invocations must all reach its `barrier()` (they skip the work instead).
  After a dispatch that writes an image, `glMemoryBarrier` before anything reads it.
- A NaN or Inf in the radiance image shows up as black squares: the bloom chain spreads it over whole
  mip blocks. Values above 64512 are Inf in `r11f_g11f_b10f`, so `trace.comp` clamps what it stores; NaNs
  must be fixed at the source (find them with a temporary atomic-counter SSBO and a scripted camera tour,
  not by eye). Invert near-singular matrices only with a regularisation that has a physical meaning.
- HDR images are `r11f_g11f_b10f` (`hdrFormat`): half the bandwidth of `rgba16f`, which halved the bloom
  cost on an iGPU. The image format qualifiers in the shaders must match it.
- Enum values shared with GLSL (`ToneMapper` ↔ `TONE_MAPPER_*`) have explicit values on both sides.
- `float` on the GPU. `double` in GLSL (fp64) runs at 1/16–1/64 of the float rate on consumer GPUs: use it
  only behind a toggle and after measuring. Write formulas so float suffices (e.g. work with `r - r+`
  near the horizon, not `r`).
- Data shared between C++ and GLSL in UBOs/SSBOs uses `std140`/`std430` and has a `static_assert` on the
  C++ struct's size and offsets. Never `vec3` in a buffer block (it pads to 16 bytes): use `vec4`.
- The minimum is OpenGL **4.3**: no 4.4+ entry points without checking `GLAD_GL_VERSION_4_5` (or the
  extension) first. In particular the 4.5 direct-state-access calls (`glCreate*`, `glNamed*`, `glTexture*`)
  are off limits until the minimum is raised; use `glGen*` + bind. Set uniforms through `ShaderProgram::set`
  (`glProgramUniform*`, core since 4.1, no bind needed).
- Performance claims are measured in a Release build with the panel's GPU timings (`gl::GpuTimer`,
  per pass), not from a Debug build or from vsync-capped FPS. Record before/after numbers in TODO.md.
- A new GPU pass gets an entry in `GpuPass`/`gpuPassNames` and an `endSection` call, in submission order.

## Code conventions

### Warnings

- **Warnings are errors.** The project's own targets build with the strict flags in
  `cmake/ProjectOptions.cmake`, and the `debug`/`asan` presets add `-Werror` (`BLACKHOLE_WERROR=ON`). A plain
  configure does not, so a newer compiler's new warning cannot break someone's build. The code must build
  warning-free with both GCC and Clang (and MSVC `/W4`), not only the compiler you used: a parameter used
  only on one platform or in one configuration is `[[maybe_unused]]`.
- **No suppression without a comment saying why** — a `#pragma GCC diagnostic`, a `NOLINT`, a cast that only
  silences a warning. `[[maybe_unused]]` on a parameter a callback signature forces on you needs none.
- Third-party code is compiled without our flags and included as `SYSTEM`; a warning from it is not ours to fix.
- The `asan` preset runs AddressSanitizer and UndefinedBehaviorSanitizer; a report there is a bug, not noise.
  GPU drivers leak at exit, so LeakSanitizer reports that point only into the driver (`radeonsi_dri.so`,
  `libnvidia-*`) can be suppressed with an LSan suppressions file; anything in our code cannot.

### Language

- No C-style or function-style casts (`(double)n`, `int(x)`); use `static_cast`, `reinterpret_cast` only at
  C APIs that need it (`glGetString` returns `const GLubyte*`), and `const_cast` only for non-const-correct
  external APIs.
- No `using namespace`, in headers or `.cpp` files.
- No `#define` in our sources: constants are `constexpr`, function-like macros are functions or templates.
  Library configuration macros (`GLFW_INCLUDE_NONE`, `IMGUI_DISABLE_OBSOLETE_FUNCTIONS`) are set by CMake
  with `target_compile_definitions`, and build-dependent constants live in the generated `config.hpp`.
- `nullptr`, never `NULL` or `0` for pointers.
- Enums are `enum class`, enumerators without a type prefix (`kerr::Orbit::PROGRADE`, not `ORBIT_PROGRADE`).
  A local `using enum` is fine in a function that is mostly case analysis on one enum.
- `explicit` on every constructor callable with one argument and on conversion operators, except a conversion
  that exists to be implicit.
- **`<algorithm>`/`std::ranges` over hand-written loops** when the loop *is* an algorithm: a search
  (`std::ranges::find`, `find_if`, with a projection instead of a lambda where one fits), `any_of`/`all_of`/
  `none_of`, `count`, `max_element`, `sort`, erase-remove, a copy into a container (range constructor or
  `insert`), and sums or folds (`std::ranges::fold_left`). Loops that do work per element, update several
  accumulators, or step through parallel arrays, pixels and numeric grids stay loops. Look a value up by key
  (`map.find`) instead of scanning the map for it.
- **Comparisons**: declare `operator==`, plus `operator<=>` only where the type has an order; C++20 rewrites
  `!=`, the reversed operands and `<`/`<=`/`>`/`>=` from them, so never write those by hand. Memberwise ones
  are `= default`. A sort or container order that is not the type's natural one is a comparator or projection
  at the call (`std::ranges::sort(v, std::ranges::greater {}, proj)`), not an `operator<`.
- **Structured bindings** to unpack pairs, tuples and result structs into named values
  (`for (const auto& [name, location] : m_locations)`), not `.first`/`.second` or a named struct read field
  by field. Assigning into existing variables or members stays `std::tie`, and a single `.second` read straight
  off a call (`seen.insert(n).second`) is fine.
- C++ standard library over the C one: `std::`-qualified `<cmath>` calls, `std::abs` rather than `fabs` or
  unqualified `abs` (which can silently resolve to the `int` overload), `std::string_view` comparisons instead
  of `strcmp`, `std::from_chars` instead of `strtol`/`strtod`/`atoi`, `std::format` and streams instead of
  `printf`. `std::numbers::pi` rather than a hand-typed π.

### Declarations and initialisation

- **Brace initialisation** for every declaration: `T x {expr};`, members `double m_x {};`, loop counters
  `for (int i {0}; ...)`. Braces reject narrowing, so conversions are explicit `static_cast`s. Exceptions:
  - `constexpr` values use `=`;
  - parentheses where braces would pick an `initializer_list` constructor: std containers built from a size
    or another expression (`std::vector<float> v(n)`, `std::string log(length, '\0')`);
  - lambdas are `const auto f = [...](...) {...};`.
- **Designated initialisers** for aggregates built in one place with many fields
  (`FrameParams {.resolution = ..., .cameraPosition = ...}`, `WindowDesc {.title = ...}`).
- **Data members**: private and protected ones carry `m_`, public ones never do. A type with public data is a
  plain record without invariants (`SceneSettings`, `FrameParams`, `WindowDesc`, `PanelInfo`). Everything else
  keeps its data private, read through a getter named after the member (`m_distance` -> `distance()`), and
  written from outside through a setter or a mutator named for what it does (`orbit`, `zoom`,
  `reloadShaders`). A getter returns a class type by `const&`.
- **Initialise everything where it is declared.** Out-parameters of C APIs are `{}` (`GLint status {};`).
  Every scalar, enum and raw-pointer member has a default initialiser in the class body
  (`float m_distance {30.0F};`, `kerr::Orbit diskOrbit {kerr::Orbit::PROGRADE};`), so no constructor can leave
  one unset. Constructors set only what depends on their arguments or differs between constructors; one that
  would only repeat the defaults is `= default`.
- **Default to `const`**: every local, range-`for` variable and by-value parameter (in the definition) that is
  not modified, and every member function that does not logically change the object. Prefer a ternary or an
  immediately-invoked lambda over declare-then-assign in `if`/`else` (`BlackHoleRenderer`'s constructor).
  Exceptions:
  - a local that is later moved from (`std::move(x)`, `std::move(*x)`) or returned by name — `const` silently
    turns the move into a copy;
  - a member function that changes state through a handle (`Window::swapBuffers`, `ShaderProgram::set`, which
    change GL/GLFW state) stays non-`const` even though it would compile.
- **Default to `constexpr`** for variables with constant initialisers and functions that can run at compile
  time. Not for anything calling `<cmath>`: GCC folds those as a builtin extension, other compilers reject them
  before C++26. Namespace-scope constants in headers are `inline constexpr`; file-local ones go in an anonymous
  namespace.
- Float literals carry the `F` suffix (`0.5F`); mixing `float` and `double` is an explicit `static_cast`
  (`-Wdouble-promotion` is on, and varargs such as `ImGui::Text` promote `float` silently).
- **Smallest scope, latest declaration**: declare a variable just before its first use, in the innermost block
  that uses it. But not inside a loop or lambda body when its value does not change there, and not past a
  statement that could change what its initialiser reads. Declarations that belong together, such as the
  coefficients of one formula, may stay together.
- `if (const auto v {compute()}; v > 0)` when the variable exists only for that `if`/`else` chain and the
  condition is about it (`it != map.end()`). Not for one of two peers the condition compares.

### Types

- **No implicit signed/unsigned conversions** (`-Wsign-conversion` is on). Fix the types rather than casting at
  each use: `std::size_t` for indices and sizes, `int`/`GLint`/`GLsizei` where OpenGL or GLFW want them. Where
  two APIs genuinely disagree, convert once in a small named helper or at the single call into the other API.
- **Strings**: `std::string_view` for read-only parameters, `std::string` by value then `std::move` for ones
  that are stored; never `const std::string&`. Maps looked up by name use `std::less<>` so a `string_view`
  finds without allocating (`ShaderProgram::m_locations`). Paths are `std::filesystem::path`.
- **Containers**: `std::array` over C arrays, `std::vector` over raw dynamic arrays. `std::span` for contiguous
  ranges a function only reads (`std::span<const ShaderFile>`). Raw pointers into data only at C API
  boundaries (`glm::value_ptr`, `.data()` passed to OpenGL).
- **Math types**: GLM (`glm::vec3`, `glm::mat3`) on the C++/GPU boundary and in rendering code; `double`
  scalars in `src/physics`.

### Ownership and special members

- **No raw `new`/`delete`.** Owning pointers are `std::unique_ptr` (`std::make_unique`, or with a deleter for C
  handles: `std::unique_ptr<GLFWwindow, Destroyer>`); a member that doesn't need the heap is a plain member.
  OpenGL names are owned by `gl::Object<Deleter>` (`gl::Program`, `gl::VertexArray`, ...), never a bare `GLuint`
  member. Raw pointers are non-owning only.
- **Rule of Zero**: no destructor or copy/move operations unless the class manages a resource (`gl::Object`,
  `GlfwLibrary`, `ImGuiLayer`) or is a polymorphic base. If you declare one, declare all five and `= default`
  or `= delete` each explicitly. A class whose address is registered somewhere (`Window`, the GLFW user
  pointer) deletes its moves. A polymorphic base keeps its copy/move `protected`.
- **`noexcept`** on every declared move constructor, move assignment and `swap`, defaulted ones included, and
  on deleters. `std::vector` reallocates with `move_if_noexcept`, so a throwing move turns every regrowth into
  a copy — or, for a move-only GL handle, a compile error.
- **Member order is construction order**: a member that needs a GL context is declared after the `Window`
  that creates it, so it is also destroyed before the context goes away (`Application`).
- **Virtuals**: `override` on every overriding function, never `virtual` together with `override`
  (`-Wsuggest-override` catches a missing one). A class nothing derives from is `final`, which lets calls
  through it devirtualise. No default arguments on virtual functions: they bind to the static type, not the
  one that is called.
- **Incomplete types**: when a `std::unique_ptr` member points to a type only forward-declared in the header,
  declare the destructor and moves in the header and `= default` them in the `.cpp`.

### Functions

- **Return values, not out-parameters**: a struct for several results (`PanelActions`, `FrameParams`),
  `std::optional<T>` for "may be absent", `std::expected<T, std::string>` for "may fail, and here is why"
  (`ShaderProgram::fromFiles`, `BlackHoleRenderer::reloadShaders`), `std::variant` when the result can be one
  of several kinds. Exceptions only for failures that end the program (no GL 4.3 context, startup shaders do
  not compile); `main` catches them. Non-const reference parameters only for values a function updates in
  place (`drawControlPanel(SceneSettings&, ...)`) or appends to.
- **Trailing return type in two cases**, everything else keeps the leading one:
  - an out-of-class definition whose return type is a member type of its own class or namespace, since the
    trailing type is looked up in `X`: `auto X::f(...) -> Nested`;
  - a return type with a template argument that is itself a template, so the name comes first and the type
    last: `auto lights() const -> const std::vector<std::shared_ptr<Light>>&` (`override`, `final` and `= 0`
    follow the type).
- **A named struct when the type does not say what the value is**: `std::pair<bool, bool>` hides what the
  flags are, `PanelActions {.reloadShaders}` does not. No type aliases for container compositions: the
  spelled-out type reads better than one more name to look up. Aliases that name a concept are fine
  (`gl::Program = gl::Object<detail::ProgramDeleter>`).
- **No sentinel values** (`-1`, `0.0`, `""`) for "nothing" in our own interfaces: status codes are `bool` or an
  `enum class`, absence is `std::optional`. OpenGL's own sentinels (name `0`, uniform location `-1`) stay
  inside the `gfx` wrappers.
- **`[[nodiscard]]`** on every function whose return value is the point, and on result structs
  (`PanelActions`). A function whose return value callers routinely ignore returns `void` instead. Exempt:
  stream-returning functions (`print(std::ostream&)`, `operator<<`) and compound assignment operators.

### Style

- 4-space indent, braces on the same line except for function definitions, line length ≤ 120.
- `#pragma once` in headers. Includes: own header first in a `.cpp`, then project headers (`"gfx/Window.hpp"`,
  rooted at `src/`), then third-party (`<glad/gl.h>` before `<GLFW/glfw3.h>`), then the standard library;
  groups separated by a blank line.
- Everything lives in `namespace bh` (sub-namespaces `bh::kerr`, `bh::gl`, `bh::config`); file-local helpers
  in an anonymous namespace.
- Comments say why and cite physics sources; the code says what.
