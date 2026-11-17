#pragma once
// Closed-form properties of the Kerr spacetime.
//
// Geometric units G = c = M = 1: lengths are in units of GM/c^2, times of GM/c^3.
// The spin parameter a = J/(Mc) is dimensionless here, 0 <= a <= 1. Radii are
// Boyer-Lindquist r. Every function's precondition is 0 <= a <= 1.

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
// far away. A spinning hole's shadow is not a circle; see TODO.md, Stage 3.
inline constexpr double schwarzschildCriticalImpactParameter = 5.196152422706632;

} // namespace bh::kerr
