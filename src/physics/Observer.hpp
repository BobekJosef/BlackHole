#pragma once
// Observers the camera can be. The renderer builds its frame around any unit 4-velocity
// (KerrSchild.hpp, observerTetrad) and normalises every ray to it, so a moving camera sees aberration
// and Doppler shifts with no further work: the redshifts of disk and sky are 1 / (p . u_source) for rays
// with p . u_camera = 1.

#include "physics/Geodesic.hpp"
#include "physics/Kerr.hpp"
#include "physics/KerrSchild.hpp"

#include <optional>

namespace bh::kerr {

// 4-velocity of an observer circling at the Keplerian angular velocity Omega(r) (Kerr.hpp) of its
// Boyer-Lindquist radius, at constant r and theta: u = u^t (d_t + Omega d_phi) with d_phi = (-y, x, 0) in
// Kerr-Schild coordinates (t and phi differ from Boyer-Lindquist by functions of r only). On the equator
// that is a circular geodesic; off it, an observer held on its circle by a force (e.g. a rocket).
// Empty where the motion is not timelike (inside the photon orbit).
[[nodiscard]] std::optional<FourVector> keplerianVelocity(const ThreeVector& position, double a, Orbit orbit);

// Speed relative to the local ZAMO as a fraction of c: v = sqrt(1 - 1 / gamma^2), gamma = -u . u_ZAMO.
[[nodiscard]] double speedRelativeToZamo(const ThreeVector& position, const FourVector& velocity, double a);

// A freely falling observer, as the state of a timelike geodesic: covariant momentum p_mu = u_mu (unit
// mass), so H = -1/2 and the affine parameter is proper time. Launched from the local ZAMO with speed
// `sidewaysSpeed` (fraction of c, |v| < 1; + in the hole's sense of rotation) along the unit phi direction
// e_phi = d_phi / |d_phi|, which is orthogonal to the ZAMO (it has no angular momentum):
// u = gamma (u_ZAMO + v e_phi). v = 0 drops it from rest with zero angular momentum (frame dragging alone
// then makes it orbit as it falls); v = the local circular speed puts it on a circular orbit; in between
// it plunges in a spiral. On the spin axis, where phi is undefined, v is ignored.
[[nodiscard]] Photon launchedFromZamo(const ThreeVector& position, double a, double sidewaysSpeed);
[[nodiscard]] Photon fallingFromRest(const ThreeVector& position, double a);

// Advances a timelike geodesic by proper time tau with RK4 sub-steps small against the local dynamical
// time r^1.5, and against r - r+ near the horizon. Stops early once r <= stopRadius, which must lie
// outside the horizon: outgoing Kerr-Schild coordinates are singular on the future horizon, which an
// infalling observer reaches (its components grow as 1 / (r - r+)), so the check has to be per sub-step.
struct [[nodiscard]] ProperTimeStep {
    Photon state {};
    double elapsed {}; // proper time actually advanced, <= tau
};
ProperTimeStep advanceProperTime(Photon state, double a, double tau, double stopRadius);

// u^mu = g^{mu nu} p_nu of a timelike geodesic state.
[[nodiscard]] FourVector observerVelocity(const Photon& state, double a);

} // namespace bh::kerr
