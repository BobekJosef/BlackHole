# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

BlackHole is an interactive, physically based renderer of a rotating (Kerr) black hole with an optional
accretion disk and a configurable background. The rendering runs on the GPU (OpenGL 4.3 core, GLSL);
the C++ side owns the window, input, UI and the closed-form physics. `TODO.md` is the roadmap. The
current shader is the Stage 0 flat-space placeholder: rays are straight lines, there is no lensing yet.

Stack: C++23, CMake ≥ 3.25, OpenGL 4.3 core, GLFW 3.4, GLAD 2 (vendored), GLM 1.0, Dear ImGui 1.91.
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
serially otherwise. Running the app opens a window on the user's display; keep such runs short and kill
the process when done.

- Dependencies come from `cmake/Dependencies.cmake`: GLFW and GLM through `find_package` when an installed
  package (system or vcpkg) matches, otherwise FetchContent; ImGui always through FetchContent; GLAD is
  generated code committed in `external/glad` (regeneration command in its README).
- Linux needs the X11 headers for GLFW: `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev`,
  and `libgl-dev`. GLFW's Wayland backend is off by default (`-DGLFW_BUILD_WAYLAND=ON` enables it).
- Shaders are read at run time from the source tree (`config::shaderDir`, generated into
  `build/*/src/generated/blackhole/config.hpp`), so a shader edit needs no rebuild: press **R** in the app
  to recompile. A failed recompile keeps the previous program and shows the log in the panel.
- `glslangValidator shaders/*.vert shaders/*.frag` checks GLSL without running the app.
- Debug builds create a debug GL context; `gl::enableDebugOutput` prints every driver message above
  "notification" to stderr. Any such message is a bug.

## Layout

```
src/physics/   closed-form Kerr formulas (bh_physics library; no OpenGL, unit-tested)
src/gfx/       thin OpenGL/GLFW wrappers: Window, GlObject (RAII names), ShaderProgram, GlDebug
src/render/    BlackHoleRenderer and FrameParams (everything uploaded per frame)
src/scene/     OrbitCamera, SceneSettings (what the UI edits)
src/ui/        ImGuiLayer (context + backends), ControlPanel
src/app/       Application: owns everything, runs the frame loop
shaders/       GLSL sources
tests/         CTest executables (bh_physics only; no GL context in tests)
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
- World frame: spin axis `+z`, equatorial plane `z = 0`. Boyer–Lindquist `(t, r, θ, φ)` for formulas and
  readouts; Kerr–Schild Cartesian coordinates for the geodesic integrator (regular at the horizon and on
  the axis).
- Every formula cites its source next to it (paper and equation number). Prefer a closed form with a test
  against the known limits (`a = 0`, `a = 1`, prograde vs retrograde) over a fitted number.
- Anything not physical (artistic exposure, fitted colours, procedural stars) is marked as such in a comment,
  and the physically meaningful path stays the default.

## GPU and GLSL conventions

- `#version 430 core` in every shader. Uniforms `uCamelCase`, stage inputs/outputs `vCamelCase`, constants
  `UPPER_SNAKE_CASE`; functions and locals `camelCase` as in C++.
- `float` on the GPU. `double` in GLSL (fp64) runs at 1/16–1/64 of the float rate on consumer GPUs: use it
  only behind a toggle and after measuring. Write formulas so float suffices (e.g. work with `r - r+`
  near the horizon, not `r`).
- Data shared between C++ and GLSL in UBOs/SSBOs uses `std140`/`std430` and has a `static_assert` on the
  C++ struct's size and offsets. Never `vec3` in a buffer block (it pads to 16 bytes): use `vec4`.
- The minimum is OpenGL **4.3**: no 4.4+ entry points without checking `GLAD_GL_VERSION_4_5` (or the
  extension) first. In particular the 4.5 direct-state-access calls (`glCreate*`, `glNamed*`, `glTexture*`)
  are off limits until the minimum is raised; use `glGen*` + bind. Set uniforms through `ShaderProgram::set`
  (`glProgramUniform*`, core since 4.1, no bind needed).
- Performance claims are measured in a Release build (GL timer queries once they exist, TODO.md Stage 1),
  not judged from a Debug build or from vsync-capped FPS.

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
