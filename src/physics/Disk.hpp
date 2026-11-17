#pragma once
// Geometrically thin, optically thick accretion disk in the equatorial plane (Novikov & Thorne 1973):
// gas on circular geodesics down to the ISCO, zero torque there.

#include "physics/Kerr.hpp"

#include <cstddef>
#include <vector>

namespace bh::kerr {

// Energy flux from one face of the disk at Boyer-Lindquist radius r, per unit accretion rate, in units
// M = Mdot = 1 (Page & Thorne 1974, eq. 15n, in the closed form with x = sqrt(r)):
//
//   F = 3 / (8 pi) / (x^4 (x^3 - 3x + 2a)) [x - x0 - 3/2 a ln(x/x0) - sum_i 3 (x_i - a)^2 / (x_i prod_{j!=i} (x_i - x_j)) ln((x - x_i)/(x0 - x_i))]
//
// with x0 = sqrt(r_ISCO) and x_i the roots of x^3 - 3x + 2a = 0. A retrograde disk is a -> -a.
// Zero at and inside the ISCO; tends to the Newtonian 3/(8 pi r^3) (1 - sqrt(r_ISCO / r)) far out.
[[nodiscard]] double novikovThorneFlux(double r, double a, Orbit orbit);

// Radius of the flux maximum (golden-section search; the profile is unimodal).
[[nodiscard]] double novikovThorneFluxPeakRadius(double a, Orbit orbit);

// Local effective temperature relative to its maximum, (F / F_max)^(1/4), sampled at
// r = rIn + (rOut - rIn) u^2 for u = i / (samples - 1): quadratic spacing resolves the steep rise
// just outside the ISCO. The renderer looks it up with u = sqrt((r - rIn) / (rOut - rIn)).
[[nodiscard]] std::vector<double> diskTemperatureProfile(double a, Orbit orbit, double rOut, std::size_t samples);

// Redshift g = nu_obs / nu_em of light from the disk gas at Boyer-Lindquist radius r (equator), for a
// ray with conserved p_t and L_z = x p_y - y p_x, normalised so that the observer measures frequency 1
// (true for the renderer's camera rays, p = -u + n). The gas moves on u = u^t (d_t + Omega d_phi),
// Omega = +-1 / (r^1.5 +- a):
//   nu_em = u^t (p_t + Omega L_z),   1 / (u^t)^2 = 1 - Omega^2 (r^2 + a^2) - 2/r (1 - a Omega)^2.
// Split through the ZAMO at the emitter (lapse alpha = r sqrt(Delta / A), frame dragging
// omega = 2 a r / A, A = (r^2 + a^2)^2 - a^2 Delta):  nu_ZAMO = (p_t + omega L_z) / alpha,
//   gravitational = 1 / nu_ZAMO (includes frame dragging),  doppler = nu_ZAMO / nu_em.
// shaders/common/disk.glsl has the float copy.
struct DiskRedshift {
    double gravitational {};
    double doppler {};
    [[nodiscard]] double total() const { return gravitational * doppler; }
};
[[nodiscard]] DiskRedshift diskRedshift(double r, double a, Orbit orbit, double pt, double angularMomentum);

// Cosine of the angle between a ray and the disk normal, measured in the rest frame of the gas (what limb
// darkening depends on), for a ray with covariant momentum p_z at the disk (same normalisation as above).
// The unit normal is e_theta, orthogonal to the gas velocity, which has only t and phi components. On the
// equator of Kerr-Schild Cartesian coordinates d_theta = -r d_z and g_thetatheta = Sigma = r^2, so the
// photon's normal component is |p_z| and  mu = |p_z| / nu_em = |p_z| g.
[[nodiscard]] double diskEmissionCosine(double r, double a, Orbit orbit, double pt, double angularMomentum, double pz);

// Limb darkening of an electron-scattering atmosphere (Chandrasekhar 1960, Radiative Transfer, sec. 68 and
// Table XXIV; the linear fit I(mu) ~ 1 + 2.06 mu as used e.g. by Laor & Netzer 1989), normalised so the
// emitted flux is unchanged: integral_0^1 L(mu) mu dmu = 1/2, i.e. L = (1 + 2.06 mu) / (1 + 4.12 / 3).
// Edge-on views of the gas (mu -> 0) are 2.4 times dimmer than face-on ones (mu = 1) at the same T.
[[nodiscard]] double electronScatteringLimbDarkening(double mu);

// Opacity of one crossing of the disk at Boyer-Lindquist radius r, seen at emission cosine mu (above).
// A model of the outer edge: instead of ending abruptly at rOut the disk thins out over the last
// `softness` fraction of its radius, its face-on opacity tapering smoothly (smoothstep) from 1 to 0. That
// fixes the face-on optical depth, tau = -ln(1 - taper); a slab of optical depth tau seen at cosine mu
// transmits exp(-tau / mu) = (1 - taper)^(1/mu), so grazing views stay more opaque. Exact radiative
// transfer for a slab; the taper's shape is the model. 1 inside the taper, 0 beyond rOut.
[[nodiscard]] double diskEdgeOpacity(double r, double rOut, double softness, double mu);

} // namespace bh::kerr
