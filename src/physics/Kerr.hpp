#pragma once
// Closed-form properties of the Kerr spacetime.
//
// Geometric units G = c = M = 1: lengths are in units of GM/c^2, times of GM/c^3.
// The spin parameter a = J/(Mc) is dimensionless here, 0 <= a <= 1. Radii are
// Boyer-Lindquist r. Every function's precondition is 0 <= a <= 1.

#include <cstddef>
#include <vector>

namespace bh::kerr {

// Sense of a circular equatorial orbit relative to the hole's rotation.
enum class Orbit { PROGRADE, RETROGRADE };

// Event horizon r+ = 1 + sqrt(1 - a^2).
[[nodiscard]] double outerHorizonRadius(double a);

// Cauchy horizon r- = 1 - sqrt(1 - a^2).
[[nodiscard]] double innerHorizonRadius(double a);

// Outer boundary of the ergoregion (static limit) at polar angle theta:
// r_E = 1 + sqrt(1 - a^2 cos^2 theta).
[[nodiscard]] double ergosphereRadius(double a, double theta);

// Innermost stable circular orbit (Bardeen, Press & Teukolsky 1972, eq. 2.21):
// 6 for a = 0; 1 (prograde) and 9 (retrograde) for a = 1.
[[nodiscard]] double iscoRadius(double a, Orbit orbit);

// Circular photon orbit in the equatorial plane (BPT 1972, eq. 2.18):
// r_ph = 2 (1 + cos(2/3 arccos(-+a))); 3 for a = 0.
[[nodiscard]] double photonOrbitRadius(double a, Orbit orbit);

// Angular velocity d(phi)/dt of a circular equatorial geodesic as seen from infinity:
// Omega = +-1 / (r^(3/2) +- a).
[[nodiscard]] double keplerianAngularVelocity(double r, double a, Orbit orbit);

// Critical impact parameter of a Schwarzschild (a = 0) hole, 3 sqrt(3): rays with a
// smaller impact parameter are captured, so it is the radius of the shadow seen from
// far away. A spinning hole's shadow is not a circle: see shadowOutline.
inline constexpr double schwarzschildCriticalImpactParameter = 5.196152422706632;

// Constants of motion of the spherical photon orbit at radius r (Bardeen 1973; requires a > 0 and
// r between the prograde and retrograde equatorial photon orbits): xi = L_z / E and eta = Q / E^2,
// xi  = (r^2 (3 - r) - a^2 (r + 1)) / (a (r - 1)),
// eta = r^3 (4 a^2 - r (r - 3)^2) / (a^2 (r - 1)^2).
struct SphericalPhotonOrbit {
    double xi {};
    double eta {};
};
[[nodiscard]] SphericalPhotonOrbit sphericalPhotonOrbit(double r, double a);

// A point of the shadow's edge on a distant observer's screen, in units of M: alpha to the
// observer's right, beta up (along the projection of the spin axis).
struct ShadowPoint {
    double alpha {};
    double beta {};
};

// Edge of the shadow seen by a distant observer at polar angle `inclination` (Bardeen 1973):
// alpha = -xi / sin(theta_o), beta = +-sqrt(eta + a^2 cos^2 theta_o - xi^2 cot^2 theta_o), over the
// spherical photon orbits. A closed loop: the upper half left to right, then the lower half back.
// The hole rotates counterclockwise seen from +z, so the flattened (prograde) side is on the left.
[[nodiscard]] std::vector<ShadowPoint> shadowOutline(double a, double inclination, std::size_t samples);

} // namespace bh::kerr
