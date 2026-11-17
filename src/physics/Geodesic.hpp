#pragma once
// Null geodesics of the Kerr metric in Kerr-Schild Cartesian coordinates (see KerrSchild.hpp),
// integrated in Hamiltonian form:
//
//   H = 1/2 g^{mu nu} p_mu p_nu = 1/2 (-p_t^2 + |p|^2 - f L^2) = 0,   L = l^mu p_mu = -p_t + l . p,
//   dx^i/dlambda = dH/dp_i  = p_i - f L l_i,
//   dp_i/dlambda = -dH/dx^i = 1/2 L^2 df/dx^i + f L d(l . p)/dx^i,
//
// with p_t conserved (stationarity) and the gradients in closed form. This is the double-precision
// CPU reference; shaders/common/kerr.glsl implements the same equations in float.

#include "physics/KerrSchild.hpp"

namespace bh::kerr {

// Spatial position and covariant spatial momentum p_i; p_t is constant along the ray.
struct Photon {
    ThreeVector position {};
    ThreeVector momentum {};
    double pt {};
};

struct PhotonDerivative {
    ThreeVector position {};
    ThreeVector momentum {};
};

[[nodiscard]] PhotonDerivative geodesicDerivative(const Photon& photon, double a);

// H; zero for a null ray, so its size measures the integration error.
[[nodiscard]] double hamiltonian(const Photon& photon, double a);

// A null momentum with p_t = pt along a spatial `direction` (need not be unit): solves H = 0 for
// the magnitude. Used to start rays far from the hole in tests.
[[nodiscard]] Photon photonAlong(const ThreeVector& position, const ThreeVector& direction, double pt, double a);

// The ray through a camera pixel. `direction` is a unit vector in the camera frame (x right,
// y up, z forward). The photon that reaches the camera from there travels along -n and has
// 4-momentum nu (u - n). We follow it backwards in time, from the camera to its source, with the
// reversed tangent -u + n: a past-directed null vector, so p_t > 0 outside the ergosphere.
// (Tracing u + n instead is only equivalent in static spacetimes; here it would show the hole
// spinning the other way.)
[[nodiscard]] Photon cameraRay(const ThreeVector& position, const Tetrad& tetrad, const ThreeVector& direction,
                               double a);

enum class RayFate { CAPTURED, ESCAPED, STEP_LIMIT };

struct TraceSettings {
    // Affine step h = r min(stepScale sqrt(r), maxStepFraction): fine near the hole, growing with r^1.5
    // where the field is weak, but never more than a fraction of r, so an incoming ray cannot jump past.
    double stepScale {0.01};
    double maxStepFraction {0.05}; // 0.25 is plenty for images; H drift is then ~1e-6 instead of ~1e-10
    int maxSteps {200000};
    double escapeRadius {1.0e4};  // Boyer-Lindquist r beyond which the ray counts as escaped
    double horizonMargin {1.01};  // captured once r < horizonMargin * r+
};

struct [[nodiscard]] TraceResult {
    RayFate fate {RayFate::STEP_LIMIT};
    Photon photon {};
    int steps {};
};

// Classical fourth-order Runge-Kutta with the step above.
TraceResult traceRay(Photon photon, double a, const TraceSettings& settings);

} // namespace bh::kerr
