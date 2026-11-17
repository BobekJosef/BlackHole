#include "physics/Geodesic.hpp"

#include "physics/Kerr.hpp"

#include <algorithm>
#include <cmath>

namespace bh::kerr {

namespace {

[[nodiscard]] ThreeVector axpy(const double h, const ThreeVector& d, const ThreeVector& x)
{
    return {x[0] + h * d[0], x[1] + h * d[1], x[2] + h * d[2]};
}

[[nodiscard]] Photon advance(const Photon& photon, const double h, const PhotonDerivative& d)
{
    return {.position = axpy(h, d.position, photon.position),
            .momentum = axpy(h, d.momentum, photon.momentum),
            .pt = photon.pt};
}

[[nodiscard]] double dot3(const ThreeVector& u, const ThreeVector& v)
{
    return u[0] * v[0] + u[1] * v[1] + u[2] * v[2];
}

} // namespace

PhotonDerivative geodesicDerivative(const Photon& photon, const double a)
{
    const auto [x, y, z] {photon.position};
    const auto [px, py, pz] {photon.momentum};
    const double a2 {a * a};

    const double r {boyerLindquistRadius(photon.position, a)};
    const double r2 {r * r};
    const double s {r2 + a2};
    const double q {r2 * r2 + a2 * z * z};
    const double f {2.0 * r2 * r / q};
    // Outgoing l_i = -(ingoing l_i with a -> -a); b = -a below.
    const double b {-a};
    const ThreeVector l {-(r * x + b * y) / s, -(r * y - b * x) / s, -z / r};
    const double lp {dot3(l, photon.momentum)};
    const double bigL {-photon.pt + lp};

    // Gradient of r (implicit differentiation of r^4 - (rho^2 - a^2) r^2 - a^2 z^2 = 0).
    const ThreeVector gradR {r2 * r * x / q, r2 * r * y / q, r * z * s / q};
    // Gradient of f = 2 r^3 / q: df = 2 r^2 [(3 a^2 z^2 - r^4) dr - 2 a^2 r z dz] / q^2, written without
    // q^2 ~ r^8, which overflows float (the GLSL copy) beyond r ~ 1e4.
    const double fFactor {f * (3.0 * a2 * z * z - r2 * r2) / (r * q)};
    const ThreeVector gradF {fFactor * gradR[0], fFactor * gradR[1], fFactor * gradR[2] - 2.0 * f * a2 * z / q};
    // Gradient of l . p = -[(r A + b B) / s + z p_z / r], A = x p_x + y p_y, B = y p_x - x p_y.
    const double bigA {x * px + y * py};
    const double bigB {y * px - x * py};
    const double c {bigA / s - 2.0 * r * (r * bigA + b * bigB) / (s * s) - z * pz / r2};
    const ThreeVector gradLp {-(c * gradR[0] + (r * px - b * py) / s), -(c * gradR[1] + (r * py + b * px) / s),
                              -(c * gradR[2] + pz / r)};

    const double fL {f * bigL};
    return PhotonDerivative {
        .position = {px - fL * l[0], py - fL * l[1], pz - fL * l[2]},
        .momentum = {0.5 * bigL * bigL * gradF[0] + fL * gradLp[0], 0.5 * bigL * bigL * gradF[1] + fL * gradLp[1],
                     0.5 * bigL * bigL * gradF[2] + fL * gradLp[2]},
    };
}

double hamiltonian(const Photon& photon, const double a)
{
    const Metric gInverse {inverseMetric(photon.position, a)};
    const FourVector p {photon.pt, photon.momentum[0], photon.momentum[1], photon.momentum[2]};
    double h {0.0};
    for (std::size_t mu {0}; mu < 4; ++mu) {
        for (std::size_t nu {0}; nu < 4; ++nu) {
            h += gInverse[mu][nu] * p[mu] * p[nu];
        }
    }
    return 0.5 * h;
}

Photon photonAlong(const ThreeVector& position, const ThreeVector& direction, const double pt, const double a)
{
    // p = k d with |d| = 1: H = 0 is (1 - f c^2) k^2 + 2 f pt c k - (1 + f) pt^2 = 0, c = l . d.
    const double length {std::sqrt(dot3(direction, direction))};
    const ThreeVector d {direction[0] / length, direction[1] / length, direction[2] / length};
    const Metric g {metric(position, a)};
    // l_mu l_nu f = g_{mu nu} - eta_{mu nu}; recover f and l from the t row: g_tt = -1 + f, g_ti = f l_i.
    const double f {g[0][0] + 1.0};
    const double c {(g[0][1] * d[0] + g[0][2] * d[1] + g[0][3] * d[2]) / f};
    const double quadratic {1.0 - f * c * c};
    const double linear {2.0 * f * pt * c};
    const double constant {-(1.0 + f) * pt * pt};
    const double k {(-linear + std::sqrt(linear * linear - 4.0 * quadratic * constant)) / (2.0 * quadratic)};
    return Photon {.position = position, .momentum = {k * d[0], k * d[1], k * d[2]}, .pt = pt};
}

Photon cameraRay(const ThreeVector& position, const Tetrad& tetrad, const ThreeVector& direction, const double a)
{
    FourVector p {};
    for (std::size_t mu {0}; mu < 4; ++mu) {
        p[mu] = -tetrad.time[mu] + direction[0] * tetrad.right[mu] + direction[1] * tetrad.up[mu]
              + direction[2] * tetrad.forward[mu];
    }
    const FourVector pLower {lower(metric(position, a), p)};
    return Photon {.position = position, .momentum = {pLower[1], pLower[2], pLower[3]}, .pt = pLower[0]};
}

TraceResult traceRay(Photon photon, const double a, const TraceSettings& settings)
{
    const double captureRadius {settings.horizonMargin * outerHorizonRadius(a)};
    for (int step {0}; step < settings.maxSteps; ++step) {
        const double r {boyerLindquistRadius(photon.position, a)};
        if (r < captureRadius) {
            return TraceResult {.fate = RayFate::CAPTURED, .photon = photon, .steps = step};
        }
        if (r > settings.escapeRadius) {
            return TraceResult {.fate = RayFate::ESCAPED, .photon = photon, .steps = step};
        }
        const double h {r * std::min(settings.stepScale * std::sqrt(r), settings.maxStepFraction)};
        const PhotonDerivative k1 {geodesicDerivative(photon, a)};
        const PhotonDerivative k2 {geodesicDerivative(advance(photon, 0.5 * h, k1), a)};
        const PhotonDerivative k3 {geodesicDerivative(advance(photon, 0.5 * h, k2), a)};
        const PhotonDerivative k4 {geodesicDerivative(advance(photon, h, k3), a)};
        for (std::size_t i {0}; i < 3; ++i) {
            photon.position[i] +=
                h / 6.0 * (k1.position[i] + 2.0 * k2.position[i] + 2.0 * k3.position[i] + k4.position[i]);
            photon.momentum[i] +=
                h / 6.0 * (k1.momentum[i] + 2.0 * k2.momentum[i] + 2.0 * k3.momentum[i] + k4.momentum[i]);
        }
    }
    return TraceResult {.fate = RayFate::STEP_LIMIT, .photon = photon, .steps = settings.maxSteps};
}

} // namespace bh::kerr
