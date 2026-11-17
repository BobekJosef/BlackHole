#pragma once
// The Kerr metric in outgoing Kerr-Schild Cartesian coordinates (t, x, y, z), M = 1:
//
//   g_{mu nu} = eta_{mu nu} + f l_mu l_nu,      g^{mu nu} = eta^{mu nu} - f l^mu l^nu,
//   f   = 2 r^3 / (r^4 + a^2 z^2),
//   l_mu = (1, -(r x - a y) / (r^2 + a^2), -(r y + a x) / (r^2 + a^2), -z / r),
//
// with eta = diag(-1, 1, 1, 1) and r the Boyer-Lindquist radius, the root of
// r^4 - (x^2 + y^2 + z^2 - a^2) r^2 - a^2 z^2 = 0.
//
// Why "outgoing": the renderer follows light backwards in time, from the camera to its source.
// A backward ray that is captured approaches the PAST horizon, and outgoing coordinates are the
// ones regular there (and on the spin axis). The usual ingoing form (l_i with the opposite sign
// and a -> -a, regular on the future horizon) makes such rays blow up just before capture.
//
// The hole rotates in the +phi sense (counterclockwise seen from +z), as Boyer-Lindquist with
// a > 0. t_KS = t_BL - int 2r/Delta dr and phi_KS = phi_BL + function(r), so d/dt and
// d/dphi = (-y, x, 0) are the same Killing vectors in both systems: E = -p_t and
// L_z = x p_y - y p_x are the usual conserved energy and angular momentum.
//
// Index 0 of a FourVector is t, 1-3 are x, y, z.

#include <array>

namespace bh::kerr {

using ThreeVector = std::array<double, 3>;
using FourVector = std::array<double, 4>;
using Metric = std::array<FourVector, 4>;

[[nodiscard]] double boyerLindquistRadius(const ThreeVector& position, double a);

// g_{mu nu}
[[nodiscard]] Metric metric(const ThreeVector& position, double a);
// g^{mu nu}
[[nodiscard]] Metric inverseMetric(const ThreeVector& position, double a);

[[nodiscard]] FourVector lower(const Metric& g, const FourVector& v);
[[nodiscard]] double dot(const Metric& g, const FourVector& u, const FourVector& v);

// Orthonormal frame of an observer: time is its 4-velocity, the spatial vectors span its rest
// space. Contravariant components.
struct Tetrad {
    FourVector time {};
    FourVector right {};
    FourVector up {};
    FourVector forward {};
};

// Frame of the zero-angular-momentum observer (ZAMO, Bardeen, Press & Teukolsky 1972) at
// `position`, u_mu = -alpha grad_mu t_BL: it co-rotates with the frame dragging, at constant r and
// theta. The spatial axes are the given coordinate directions made orthonormal by Gram-Schmidt in
// the order forward, up, right, so "forward" is exact. Requires r > r+.
[[nodiscard]] Tetrad zamoTetrad(const ThreeVector& position, const ThreeVector& right, const ThreeVector& up,
                                const ThreeVector& forward, double a);

// Angular velocity d(phi)/dt of the ZAMO (frame dragging), Boyer-Lindquist:
// omega = 2 a r / ((r^2 + a^2)^2 - a^2 Delta sin^2 theta).
[[nodiscard]] double frameDraggingAngularVelocity(double r, double theta, double a);

} // namespace bh::kerr
