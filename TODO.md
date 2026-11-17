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

## Stage 1 — Rendering infrastructure

The geodesic tracer is expensive, so the frame is structured for it before the physics arrives.

- [ ] **Compute shader** traces into an `rgba16f` image (`glDispatchCompute`, 8×8 or 16×16 work groups,
      `imageStore`); a full-screen pass tone-maps it into the default framebuffer.
- [ ] **Resolution scale** slider (render at 25–100 % and upscale bilinearly) — the main performance knob.
- [ ] **HDR pipeline**: physical radiance in, ACES or AgX tone mapping out, exposure in EV.
- [ ] **Bloom** (dual-filter / Kawase down- and up-sampling chain). Real optics, not an effect: the disk's
      inner edge is orders of magnitude brighter than the rest.
- [ ] **GPU timing** with `GL_TIME_ELAPSED` queries (double-buffered so they never stall), shown per pass in
      the panel. Turn VSync off when measuring.
- [ ] **Per-frame UBO** (`std140`) instead of individual uniforms, with `static_assert`s on the C++ layout.
- [ ] **`#include` for GLSL** (a small preprocessor in `ShaderProgram`) so `kerr.glsl`, `disk.glsl`,
      `sky.glsl`, `color.glsl` are shared between passes; report errors with the original file and line.
- [ ] Auto-reload shaders when a file's modification time changes (poll once per second).

## Stage 2 — Schwarzschild lensing (`a = 0`)

A simpler problem with known answers, to validate the integrator, the camera and the termination logic
before adding spin.

- [ ] Integrate null geodesics in 3D (Cartesian Schwarzschild in Kerr–Schild form, so Stage 3 only swaps
      the metric). Alternative for a first light: each ray stays in its own plane, where `u = 1/r` obeys
      `d²u/dφ² = 3u² − u` (Binet) — a 1D ODE in the ray's plane.
- [ ] Termination: captured at `r < r+ (1 + ε)`, escaped at `r > r_escape` (≥ camera distance, ~1000 M)
      with the final direction looking up the sky, step limit as a safety net (shown as magenta in debug).
- [ ] Integrator: RK4 with a step `h ∝ r` (coarse far away, fine near the hole), then compare with an
      adaptive RK45 (Dormand–Prince / Cash–Karp) on quality vs. cost.
- [ ] Validation:
  - the shadow's angular radius seen from `r_obs` equals `arcsin(3√3 √(1 − 2/r_obs) / r_obs)`;
  - weak-field deflection `→ 4/b` for large impact parameter `b`;
  - an Einstein ring when a bright point is placed exactly behind the hole.
- [ ] Debug views: step-count heat map, termination reason, deflection angle.

## Stage 3 — Kerr geodesics (`0 ≤ a < 1`)

- [ ] **Equations of motion.** Hamiltonian form `H = ½ g^{μν} p_μ p_ν = 0`,
      `dx^μ/dλ = ∂H/∂p_μ`, `dp_μ/dλ = −∂H/∂x^μ`, in **Kerr–Schild Cartesian** coordinates
      `g_{μν} = η_{μν} + f l_μ l_ν` with `f = 2r³/(r⁴ + a²z²)` and
      `l = (1, (rx + ay)/(r² + a²), (ry − ax)/(r² + a²), z/r)`, where `r` solves
      `r⁴ − (x² + y² + z² − a²) r² − a² z² = 0`. The inverse metric is `η^{μν} − f l^μ l^ν`, so no matrix
      inversion is needed. Regular at the horizon and on the axis — unlike Boyer–Lindquist, whose
      `θ = 0, π` and `r = r+` singularities produce seams and stalls. (GRay, Chan et al. 2013, uses this
      approach on the GPU.)
- [ ] Alternative worth benchmarking: Boyer–Lindquist with the conserved `E`, `L_z` and Carter constant `Q`
      (first-order equations, Carter 1968). Faster per step, but needs sign tracking at radial and polar
      turning points.
- [ ] Use the conserved quantities as an accuracy monitor: the drift of `H`, `E = −p_t`, `L_z = p_φ` along
      a ray, shown as a debug view; adapt the step size until it is below ~1e-4 everywhere in float.
- [ ] **Camera as a physical observer.** Build an orthonormal tetrad at the camera position — the
      zero-angular-momentum observer (ZAMO, Bardeen, Press & Teukolsky 1972) — turn each pixel's direction
      `n` into the local photon momentum `p^(a) = (1, n)` and then into `p^μ = e_(a)^μ p^(a)`. Later: a
      camera moving relative to the ZAMO (Lorentz boost of the tetrad → aberration), e.g. on a circular
      orbit or falling in. See James et al. 2015 (DNGR), Appendix A, for the full construction.
- [ ] Keep the camera outside the horizon (`r > r+ + margin`); inside the ergosphere it cannot be static,
      which the ZAMO frame handles.
- [ ] **Validation: Kerr shadow outline.** Overlay the analytic critical curve (Bardeen 1973): for photon
      orbit radii `r` between the prograde and retrograde ones,
      `ξ = (r²(3 − r) − a²(r + 1)) / (a(r − 1))`, `η = r³(4a² − r(r − 3)²) / (a²(r − 1)²)`; an observer at
      infinity and inclination `θ_o` sees `α = −ξ / sin θ_o`, `β = ±√(η + a² cos²θ_o − ξ² cot²θ_o)`. The
      traced shadow edge must match it (D-shaped and shifted for high spin).
- [ ] Precision study: float everywhere vs. double (`GL_ARB_gpu_shader_fp64`) near the horizon and photon
      ring; fp64 only behind a toggle if float is visibly insufficient.

## Stage 4 — Accretion disk (optional, toggle)

- [ ] **Geometry**: geometrically thin, optically thick disk in the equatorial plane from `r_ISCO` to
      `r_out`. Detect the crossing of `z = 0` between two integration steps and locate it by linear
      interpolation or bisection.
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
