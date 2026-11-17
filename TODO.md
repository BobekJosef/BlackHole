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
- [ ] Einstein ring check against an exact lens equation (the ring is visible in Stage 5's magnification view).

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

## Stage 4 — Accretion disk (optional, toggle) ✅

- [x] **Geometry**: geometrically thin, optically thick disk in the equatorial plane from `r_ISCO` to
      `r_out`, crossing of `z = 0` located on the chord between two steps (Stage 3).
- [x] **Emission**: Page–Thorne (1974) closed-form flux `F(r)` (zero torque at the ISCO), local temperature
      `T = T_peak (F/F_max)^{1/4}`, sampled into a 1D texture on a `u²` grid that resolves the rise at the
      ISCO. Tested against a direct numerical Page–Thorne integral over the circular-orbit `E`, `L`, `Ω`
      (5 spin/orbit cases, 1e-4), zero at the ISCO, Newtonian limit far out.
- [x] **Emitter motion and redshift**: Keplerian gas, `g = 1/(u^t (p_t + Ω L_z))` from the ray's conserved
      quantities, split through the emitter's ZAMO into gravitational (with frame dragging) × Doppler.
      Tested against explicit contractions `p·u` in Kerr–Schild coordinates and the face-on Schwarzschild
      `g = √(1 − 3/r)`.
- [x] **Brightness and colour**: a black body at `T` seen with shift `g` is a black body at `gT`, so one
      look-up of `gT` in a CIE-1931-integrated Planck table (linear sRGB, Wyman–Sloan–Shirley CMF fit)
      gives the colour and the `g⁴` beaming at once. Tested: 6500 K near white, cool red, hot blue,
      Rayleigh–Jeans `Y ∝ T`.
- [x] Panel: peak temperature (`∝ (Ṁ/M²)^{1/4}`), outer radius, prograde/retrograde, Doppler and
      gravitational shift toggles (for teaching), flux-peak radius readout, *Redshift g* debug view.

No measurable cost: the shading runs once per disk pixel, after the geodesic (trace 14.1 ms as before).

Follow-ups:
- [ ] Higher-order images on demand: continue the ray through the disk plane (semi-transparent disk or a
      debug view showing the n = 1, 2 images separately).
- [x] Limb darkening of the disk atmosphere (electron scattering, `I ∝ 1 + 2.06 μ`, Chandrasekhar 1960),
      flux-conserving; `μ = |p_z| g` in the gas frame (`kerr::diskEmissionCosine`, tested against a
      projection onto the normal made orthogonal to the gas velocity).
- [x] Outer edge: the disk now reaches 100 M by default (slider to 1000 M, escape radius follows it), so it
      fades out as it cools (`T ∝ r^-3/4`) instead of ending at a visible ~1750 K rim at 20 M.
- [x] Soft outer edge (*Edge softness*, default the outer half of the radius): the disk thins out, its
      face-on opacity tapering from 1 to 0 (a model), seen through slab transfer `1 − e^(−τ/μ)`
      (`kerr::diskEdgeOpacity`, tested). Rays pick up `α·B` at a thin crossing and continue attenuated, so
      the sky and the lensed far side show through; no measurable cost (12.11 vs 12.04 ms).
- [x] Turbulence, a statistical model (marked as such): lognormal flux `F = F_NT exp(σn − σ²Var/2)` (mean kept
      at Page–Thorne), eddies of the disk thickness `H = h r` stretched 4× along the flow, advected with
      `Ω(r)` so the Keplerian shear winds them into trailing spirals, living one local orbit (two
      crossfading generations), animated in coordinate time (*Time scale*, M per second). Octaves smaller
      than the pixel's footprint on the disk (from the neighbours' hits in shared memory) fade to the mean.
      Lifetimes are quantised into octave bands `T_b = 2^b M` (picked by the local period, neighbouring
      bands blended): a per-radius lifetime, phase `t/T(r)`, has a radial phase gradient growing with `t`,
      and after ~10⁴ M the pattern had shattered into noise with axis-aligned spokes (user report;
      reproduced by starting the clock at 20000 M). Unit-length gradients and a rotation between octaves
      remove the lattice-aligned streaks. Cost 1.7 ms (trace 11.9 vs 10.2 ms with turbulence off).
- [x] Star field: the camera's own shift `g = 1/p_t` and the Planck table instead of the fitted colours (Stage 5).
- [ ] Light-travel time for the animated turbulence: integrate `dt/dλ = −p_t + f L` along the ray, so the
      far side of the disk is seen as it was ~2r earlier.
- [ ] Spectral hardening (colour-correction factor `f_col ≈ 1.7`, Shimura & Takahara 1995) for hot disks.
- [ ] Finite thickness / volumetric disk (thick flows at high accretion rates).

## Stage 5 — Background (customisable) ✅

- [x] **Ray differentials from the work group**: after tracing, the invocations of an 8×8 group exchange
      their escape directions through shared memory; the difference to the neighbouring pixel (forward,
      else backward, else the unlensed pixel size) is the pixel's footprint on the sky. No extra rays, and
      no measurable cost: trace 14.05 ms with the sky off, as before.
- [x] **Magnification** `μ = (pixel solid angle) / (sky solid angle it sees)` from that footprint, shown by
      the *Magnification (sky)* debug view: the Einstein ring is the red critical curve (μ → ∞), the sky
      inside it the demagnified secondary image, μ → 1 far away.
- [x] **Image maps**: equirectangular (`.hdr`, PNG 8/16 bit, JPEG, TGA, BMP; width = 2 × height) and cube
      maps (a folder of six faces named `px…nz`, `posx…negz` or `right…back`), decoded by stb_image on a
      worker thread (`bh_assets`, unit-tested), uploaded with mipmaps (`r11f_g11f_b10f` for float data,
      `SRGB8_ALPHA8` for 8-bit), sampled with `textureGrad` from the footprint plus anisotropic filtering
      where available. Orientation checked with labelled test maps: equirectangular unmirrored with
      φ = 0 at the image centre; cube maps need a reflection (`xzy`), not a rotation, because OpenGL's
      cube-map convention is left-handed.
- [x] **Procedural stars**, now with physical imaging: point sources whose pixel radiance is
      `F / (sky solid angle of the pixel)` (so lensing magnification brightens them along the critical
      curve, and both images of a star behind the hole show), splatted as a 0.5 px Gaussian through the
      inverse footprint Jacobian (flux-conserving at any magnification), black-body colours from the
      Planck table at `g T`. Two lattice layers (bright/sparse, faint/dense); a layer whose star spacing is
      no longer resolved fades into its mean radiance, like a mipmap. Positions and fluxes stay random.
- [x] **Coordinate grid** background (10° lines in θ and φ, equator red, φ = 0 green), box-filtered with
      the footprint: shows the lensing map, the Einstein ring and the mirrored secondary image.
- [x] **The camera's own shift of the sky**: `g = 1/p_t` (`kerr::skyRedshift`, tested: `1/√(1 − 2/r)` for
      a static Schwarzschild camera, ZAMO frequency `(E − ωL)/α = 1` in Kerr). Exact for the black-body
      stars; image maps and the grid only get the bolometric `g⁴` (an RGB image has no spectrum).
- [x] Panel: background (none / stars / grid / image), map list from `assets/` with *Rescan*, a path
      field, load status, brightness (EV), yaw about the spin axis and tilt.
- [x] `scripts/download_sky_maps.sh` fetches the ESO Milky Way panorama (CC BY 4.0, ESO/S. Brunier) into
      the git-ignored `assets/`.

Measured (Release, AMD Renoir iGPU, 1600×900, render scale 0.75, camera at 30 M, disk on): trace 14.05 ms
with no background, 14.1 ms with the ESO map (one `textureGrad` per sky pixel), 15.5 ms with procedural
stars (the per-pixel star search, two layers × ~14 lattice cells).

Follow-ups:
- [ ] OpenEXR (tinyexr): NASA SVS *Deep Star Maps 2020* are EXR. (Its server's certificate chain failed
      verification from here, so the download script does not include it.)
- [ ] A real star catalogue (HYG / Gaia bright stars) through the same point-source path, with
      temperatures from B−V: physical positions and fluxes.
- [ ] Image maps: optional mapping of the RGB image to a per-pixel colour temperature so the camera shift
      changes their colour too (approximation, clearly marked).
- [ ] Star field cost: one 3-D lattice search per layer per pixel; a cube-face 2-D grid would visit fewer
      cells.
- [ ] Einstein ring check against an exact strong-field lens equation (Stage 2 left-over), now that the
      magnification view shows it.

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
