# TODO — roadmap

A plan from the current scaffold to a real-time, physically based image of a Kerr black hole with an
optional accretion disk and a customisable background. Stages are ordered so that each one ends in
something visible and testable; later stages assume the earlier ones.

Units `G = c = M = 1`, signature `(-,+,+,+)`, spin axis `+z` (see CLAUDE.md, "Physics conventions").

---

## Stage 0 — Scaffold ✅

- [x] CMake (presets, strict warnings, sanitizers), FetchContent for GLFW / GLM / ImGui, vendored GLAD
- [x] Window + OpenGL 4.3 core context, GL debug output in Debug builds
- [x] RAII GL objects, `ShaderProgram` with `std::expected` errors and hot reload (R)
- [x] Orbit camera, ImGui control panel, FPS counter
- [x] `bh_physics`: horizon, ergosphere, ISCO, photon orbit, Keplerian Ω, with unit tests
- [x] Placeholder shader: straight rays, 3√3 M "shadow" sphere, flat disk from the ISCO, procedural stars

## Stage 1 — Rendering infrastructure ✅

The geodesic tracer is expensive, so the frame is structured for it before the physics arrives.

- [x] **Compute shader** (`trace.comp`, 8×8 work groups) traces into an HDR image; a full-screen pass
      (`present.frag`) tone-maps it into the default framebuffer.
- [x] **Resolution scale** slider (25–100 %, bilinear up-scaling) — the main performance knob.
- [x] **HDR pipeline**: linear radiance in, AgX (default) / ACES (Hill fit) / exponential / clamp out,
      exposure in EV, exact sRGB encoding, dithering against banding.
- [x] **Bloom**: Jimenez (SIGGRAPH 2014) 13-tap down-sampling chain with a Karis average on the first step,
      tent-filter up-sampling, mixed in energy-conserving (`mix`, not add).
- [x] **GPU timing** with `GL_TIMESTAMP` queries, 4 frames in flight so they never stall; per pass in the panel.
- [x] **Per-frame UBO** (`std140`, `FrameUniforms.hpp` ↔ `common/frame.glsl`) with `static_assert`ed offsets.
- [x] **`#include` for GLSL** (`ShaderPreprocessor`, unit-tested): one source-string number per file via
      `#line`, driver logs mapped back to `file:line` for Mesa, NVIDIA and AMD/Intel log formats.
- [x] Reload on save (directory polled once per second); a failed reload keeps the previous programs.
- [x] CTest validates every shader with `glslangValidator` when it is installed.

Measured (Release, 1600×900, AMD Renoir iGPU, flat-space placeholder): trace 1.5 ms, bloom 0.9 ms,
present 0.7 ms. Switching the HDR targets from `rgba16f` to `r11f_g11f_b10f` halved bloom (1.85 → 0.87 ms):
on integrated GPUs the post-processing is bandwidth-bound.

Follow-ups, when they matter:
- [ ] Present costs 0.7 ms at full HD on the iGPU: fold tone mapping into a compute pass, or precompute AgX
      as a 3D LUT, once the trace pass is no longer the bottleneck.
- [ ] Revisit `r11f_g11f_b10f` precision for the radiance image once the disk has physical (very large)
      brightness ratios; switch the radiance target alone back to `rgba16f` if banding appears.
- [ ] A `--benchmark` mode: fixed camera path, VSync off, timings written to stdout (for comparing changes).

## Stage 2 — Schwarzschild lensing (`a = 0`) ✅

Done as the `a = 0` case of the Stage 3 integrator; every Schwarzschild check is a unit test in double
precision (`tests/test_geodesic.cpp`, against `bh_physics`):

- [x] Weak-field deflection `4/b + 15π/(4b²)` (b = 100, to 1e-4).
- [x] Capture threshold `b = 3√3` (±0.2 %), for rays from infinity.
- [x] Shadow seen from a finite distance: a camera ray at `sin ψ = 3√3 √(1 − 2/r)/r` sits on the edge
      (±0.3 %, camera at r = 20).
- [x] Termination: captured below `1.01 r+`, escaped beyond `max(200, 4 × camera distance)`, step limit
      as a safety net (magenta in the debug views).
- [x] Debug views: step count, constraint error `|H|/|p|²`, fate (captured / sky / disk / step limit).
- [ ] Einstein ring check with a bright point exactly behind the hole (needs Stage 5 image backgrounds).

## Stage 3 — Kerr geodesics (`0 ≤ a < 1`) ✅

- [x] **Hamiltonian equations in outgoing Kerr–Schild Cartesian coordinates**, inverse metric in closed
      form, gradients of `r`, `f` and `l·p` derived analytically (`physics/Geodesic.cpp`) and checked
      against finite differences of `H`. Float copy in `shaders/common/kerr.glsl`.
- [x] **Outgoing, not ingoing, coordinates**: rays run backwards in time, so captured rays approach the
      *past* horizon, where the usual ingoing form is singular (H blew up to 1e10 before capture).
- [x] **Backward tracing with `p = −u + n`** (past-directed). Tracing `u + n` is only equivalent in
      static spacetimes; in Kerr it mirrors the spin. Pinned by the asymmetric camera test below.
- [x] **ZAMO camera**: tetrad built on the CPU in double (`kerr::zamoTetrad`), uploaded lowered, so a pixel's
      initial momentum is one mat4 × vec4 on the GPU. Tests: orthonormal, future-directed, constant r,
      co-rotates at the frame-dragging `ω`.
- [x] **Validation**: equatorial capture thresholds at the prograde/retrograde critical `ξ` (Bardeen 1973),
      for rays from infinity and through camera pixels (which fixes the rotation sense); the analytic
      shadow edge overlaid in the app (*Analytic shadow edge*) matches the traced edge to ~1 px at r = 30.
- [x] **Precision**: float is enough. Constraint error ~1e-7 on the disk, ~1e-5 in the sky, ~1e-4 next to
      the photon ring at step scale 0.02 (0.03: ~4e-4, 30 % faster). One pixel is ~1e-3 rad.
- [x] Escape radius: the coordinate direction at r = 200 is within 1e-5 rad of the asymptotic one (outgoing
      Kerr–Schild coordinates are built on straight null lines), so rays stop there (test).
- [x] Thin opaque disk at `z = 0` (= Boyer–Lindquist equator) between ISCO and `r_out`, hit located on the
      chord between steps: lensed far side and higher-order images come for free. Colour still placeholder.

Measured (Release, AMD Renoir iGPU, camera at 30 M, step scale 0.02): trace 32.7 ms at 1600×900 as first
written; 24.8 ms after rewriting the derivative around three reciprocals and cutting the escape radius
from 1000 to 200; 14.1 ms at the new default render scale 0.75 (66 FPS). The tracer is ALU-bound
(~1.8 TFLOP/s effective, ~100 steps × 4 derivative evaluations per pixel).

Follow-ups:
- [ ] Benchmark the Boyer–Lindquist alternative (conserved `E`, `L_z`, Carter `Q`, first-order equations,
      Carter 1968): fewer operations per step, but turning points need sign tracking.
- [ ] Adaptive step size from an embedded error estimate (RK45 / Dormand–Prince) instead of `h ∝ r^1.5`:
      most of the error is next to the photon ring, most of the cost elsewhere.
- [ ] Moving cameras: boost the ZAMO tetrad (circular orbit, free fall), giving aberration.
- [ ] Compare a CPU reference image with a GPU readback (Stage 7) to catch drift between the C++ and GLSL copies.

## Stage 4 — Accretion disk (optional, toggle)

- [x] **Geometry**: geometrically thin, optically thick disk in the equatorial plane from `r_ISCO` to
      `r_out`, crossing of `z = 0` located on the chord between two steps (done in Stage 3). Bisection if
      the chord turns out too coarse once the emission is physical.
- [ ] **Emitter motion**: circular Keplerian orbits, `u^μ = u^t (1, 0, 0, Ω)` in BL with
      `Ω = ±1/(r^{3/2} ± a)` (`kerr::keplerianAngularVelocity`) and
      `u^t = 1/√(−(g_tt + 2Ω g_tφ + Ω² g_φφ))`.
- [ ] **Redshift**: `g = ν_obs/ν_em = (p_μ u^μ)_obs / (p_μ u^μ)_em`. For the static ZAMO camera this
      reduces to a function of the photon's conserved `E`, `L_z` and the emitter's `u^t`, `Ω`. Gravitational
      redshift and Doppler shift are both in `g`; add panel toggles that drop each one for teaching.
- [ ] **Brightness**: `I_ν/ν³` is invariant, so bolometric intensity transforms as `I_obs = g⁴ I_em`.
      This gives the strong Doppler beaming (one side of the disk much brighter).
- [ ] **Emission profile**: Page–Thorne (1974) / Novikov–Thorne (1973) flux `F(r)` for a relativistic thin
      disk (zero torque at the ISCO), local temperature `T(r) = (F/σ)^{1/4}`. Implement it in `bh_physics`
      with tests against the Newtonian limit far out, and sample it into a 1D texture over `r/r_ISCO`.
- [ ] **Colour**: a blackbody at temperature `T` seen with shift `g` is a blackbody at `gT`. Precompute a
      LUT `T → linear sRGB` by integrating Planck's law against the CIE 1931 colour matching functions
      (C++ at startup, 1D texture). Replaces the fitted `blackbodyColor` from Stage 0.
- [ ] Disk parameters in the panel: accretion rate / peak temperature, outer radius, inclination is the
      camera's, prograde/retrograde (already there).
- [ ] Higher-order images come for free: let rays pass through the disk plane more than once (e.g. treat the
      disk as partly transparent, or continue tracing after a hit for a debug view showing the n = 1, 2 photon
      ring images).
- [ ] Optional, clearly marked non-physical: procedural turbulence (noise advected with `Ω(r)`), thick /
      volumetric disk, disk animation with `uTime`.

## Stage 5 — Background (customisable)

- [ ] **Equirectangular image** (`.hdr` / `.exr` / `.png` / `.jpg`) and **cubemap** backgrounds, sampled by
      the escaped ray's final direction. Loader: `stb_image` (FetchContent; `.hdr` included), optionally
      tinyexr for EXR.
- [ ] File picker in the panel (ImGui text field + list of files in `assets/`, or portable-file-dialogs),
      background rotation and brightness controls.
- [ ] Keep the procedural star field as the zero-dependency default; add a **coordinate grid sphere**
      background to make the lensing distortion obvious.
- [ ] **Antialiasing / filtering**: lensing magnifies and minifies the sky enormously near the shadow, so
      screen-space derivatives are wrong. Use mipmaps with an explicit LOD from ray differentials (trace a
      neighbouring ray or estimate the ray-bundle spread), or supersample the sky only where the step count
      is high.
- [ ] Suggested free sky maps: NASA SVS "Deep Star Maps" (public domain), ESA Gaia sky map (check licence).
      Do not commit large images: download them into `assets/` (git-ignored) with a script.

## Stage 6 — Interactivity and performance

- [ ] **Adaptive quality**: lower resolution scale while the camera moves, full resolution when it stops.
- [ ] **Temporal accumulation**: when nothing changes, jitter sub-pixel ray origins and average frames
      (progressive anti-aliasing at no interactive cost).
- [ ] Early ray termination and a tight `r_escape`; skip disk tests above `|z| > disk thickness` cheaply.
- [ ] Profile work-group size, register pressure (fewer live variables in the integrator), and branch
      divergence (rays near the photon ring take many more steps than their neighbours).
- [ ] More camera modes: free-fly (WASD), circular orbit around the hole, free fall into it (timelike
      geodesic integrated on the CPU in double, via `bh_physics`).
- [ ] Screenshot (`stb_image_write` PNG, at a chosen resolution offline), save/load settings presets
      (JSON or ImGui `.ini`), HiDPI-aware ImGui scaling.

## Stage 7 — Quality and tooling

- [ ] **CPU reference tracer** in `bh_physics` (double precision, same equations) that renders a small image;
      a test compares it pixel by pixel against the GPU output read back with `glGetTexImage`.
- [ ] Switch the hand-rolled test harness to doctest or Catch2 (FetchContent) when the tests grow.
- [ ] CI (GitHub Actions): Linux GCC + Clang and Windows MSVC builds with `-Werror`, unit tests, and
      `glslangValidator` on every shader. No GPU needed for any of these.
- [ ] `.clang-format` and `.clang-tidy` matching CLAUDE.md; LSan suppressions file for GPU-driver leaks in the
      `asan` preset.
- [ ] Bump dependency versions deliberately (GLFW 3.4, GLM 1.0.1, ImGui 1.91.9b are pinned now).

---

## References

- J. M. Bardeen, W. H. Press, S. A. Teukolsky, *Rotating black holes: locally nonrotating frames, energy
  extraction, and scalar synchrotron radiation*, ApJ 178, 347 (1972) — ISCO, photon orbits, ZAMO frame.
- J. M. Bardeen, *Timelike and null geodesics in the Kerr metric*, in *Black Holes* (Les Houches 1973) —
  shadow outline.
- B. Carter, *Global structure of the Kerr family of gravitational fields*, Phys. Rev. 174, 1559 (1968).
- I. D. Novikov, K. S. Thorne (1973); D. N. Page, K. S. Thorne, ApJ 191, 499 (1974) — thin-disk flux.
- J.-P. Luminet, *Image of a spherical black hole with thin accretion disk*, A&A 75, 228 (1979).
- C. Chan, D. Psaltis, F. Özel, *GRay: a massively parallel GPU-based code for ray tracing in relativistic
  spacetimes*, ApJ 777, 13 (2013).
- O. James, E. von Tunzelmann, P. Franklin, K. S. Thorne, *Gravitational lensing by spinning black holes in
  astrophysics, and in the movie Interstellar*, Class. Quantum Grav. 32, 065001 (2015).
- S. E. Gralla, A. Lupsasca, *Null geodesics of the Kerr exterior*, Phys. Rev. D 101, 044032 (2020) —
  analytic solutions, useful for validation.
