// Validates the Kerr-Schild metric, the ZAMO camera frame and the null-geodesic integrator against
// closed-form results:
//  - g g^-1 = 1, Minkowski far away;
//  - the ZAMO is a unit timelike observer at constant r that co-rotates with the frame dragging;
//  - Schwarzschild: weak-field deflection 4/b + 15 pi / (4 b^2), capture threshold 3 sqrt(3), and
//    the exact shadow size seen by a static observer at finite distance, sin psi = 3 sqrt(3) sqrt(1 - 2/r) / r;
//  - Kerr: capture thresholds of equatorial rays at the prograde/retrograde critical xi (Bardeen 1973),
//    both for rays from infinity and through a camera pixel, which also fixes the sense of rotation.

#include "physics/Geodesic.hpp"
#include "physics/Kerr.hpp"
#include "physics/KerrSchild.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <source_location>
#include <string_view>

namespace {

namespace kerr = bh::kerr;

int g_failures {0};

void expect(const bool condition, const std::string_view what,
            const std::source_location where = std::source_location::current())
{
    if (!condition) {
        ++g_failures;
        std::cerr << where.file_name() << ':' << where.line() << ": " << what << '\n';
    }
}

void expectNear(const double actual, const double expected, const double tolerance, const std::string_view what,
                const std::source_location where = std::source_location::current())
{
    if (!(std::abs(actual - expected) <= tolerance)) { // written so that NaN fails
        ++g_failures;
        std::cerr << where.file_name() << ':' << where.line() << ": " << what << ": got " << actual << ", expected "
                  << expected << " +- " << tolerance << '\n';
    }
}

void testMetric()
{
    constexpr double a = 0.9;
    const kerr::ThreeVector point {3.0, -2.0, 1.5};
    const kerr::Metric g {kerr::metric(point, a)};
    const kerr::Metric gInverse {kerr::inverseMetric(point, a)};
    for (std::size_t mu {0}; mu < 4; ++mu) {
        for (std::size_t nu {0}; nu < 4; ++nu) {
            double product {0.0};
            for (std::size_t k {0}; k < 4; ++k) {
                product += g[mu][k] * gInverse[k][nu];
            }
            expectNear(product, mu == nu ? 1.0 : 0.0, 1e-12, "g g^-1 = identity");
        }
    }
    const kerr::Metric far {kerr::metric({1.0e9, 0.0, 0.0}, a)};
    expectNear(far[0][0], -1.0, 1e-8, "g_tt -> -1 far away");
    expectNear(far[1][1], 1.0, 1e-8, "g_xx -> 1 far away");
    // On the equator z = 0 the Boyer-Lindquist radius is sqrt(x^2 + y^2 - a^2).
    expectNear(kerr::boyerLindquistRadius({4.0, 3.0, 0.0}, a), std::sqrt(25.0 - a * a), 1e-12, "r on the equator");
}

void testZamo()
{
    constexpr double a = 0.9;
    const kerr::ThreeVector position {4.0, 2.0, 1.0};
    const kerr::Tetrad tetrad {kerr::zamoTetrad(position, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, {-1.0, 0.0, 0.0}, a)};
    const kerr::Metric g {kerr::metric(position, a)};
    const std::array frame {tetrad.time, tetrad.right, tetrad.up, tetrad.forward};
    for (std::size_t i {0}; i < 4; ++i) {
        for (std::size_t j {0}; j < 4; ++j) {
            const double expected {i != j ? 0.0 : (i == 0 ? -1.0 : 1.0)};
            expectNear(kerr::dot(g, frame[i], frame[j]), expected, 1e-12, "tetrad is orthonormal");
        }
    }
    expect(tetrad.time[0] > 0.0, "ZAMO is future-directed");

    // No radial motion: u^i dr/dx^i = 0 (dr/dx^i by central differences).
    constexpr double eps = 1e-6;
    double drdTau {0.0};
    for (std::size_t i {0}; i < 3; ++i) {
        kerr::ThreeVector plus {position};
        kerr::ThreeVector minus {position};
        plus[i] += eps;
        minus[i] -= eps;
        drdTau += tetrad.time[i + 1]
                * (kerr::boyerLindquistRadius(plus, a) - kerr::boyerLindquistRadius(minus, a)) / (2.0 * eps);
    }
    expectNear(drdTau, 0.0, 1e-8, "ZAMO stays at constant r");

    // Angular velocity d(phi)/dt equals the frame-dragging omega(r, theta).
    const auto [x, y, z] {position};
    const double r {kerr::boyerLindquistRadius(position, a)};
    const double omega {(x * tetrad.time[2] - y * tetrad.time[1]) / (x * x + y * y) / tetrad.time[0]};
    expectNear(omega, kerr::frameDraggingAngularVelocity(r, std::acos(z / r), a), 1e-12, "ZAMO co-rotates");
    expect(omega > 0.0, "frame dragging is counterclockwise for a > 0");

    const kerr::Photon ray {kerr::cameraRay(position, tetrad, {0.3, -0.2, std::sqrt(0.87)}, a)};
    expectNear(kerr::hamiltonian(ray, a), 0.0, 1e-12, "camera ray is null");
}

// The analytic equations of motion against finite differences of H itself:
// dx/dlambda = dH/dp, dp/dlambda = -dH/dx.
void testHamiltonEquations()
{
    constexpr double a = 0.9;
    const kerr::Photon photon {.position = {2.5, -1.2, 0.8}, .momentum = {0.3, 0.7, -0.4}, .pt = 0.9};
    const kerr::PhotonDerivative derivative {kerr::geodesicDerivative(photon, a)};
    constexpr double eps = 1e-6;
    for (std::size_t i {0}; i < 3; ++i) {
        kerr::Photon plus {photon};
        kerr::Photon minus {photon};
        plus.momentum[i] += eps;
        minus.momentum[i] -= eps;
        const double dHdp {(kerr::hamiltonian(plus, a) - kerr::hamiltonian(minus, a)) / (2.0 * eps)};
        expectNear(derivative.position[i], dHdp, 1e-7, "dx/dlambda = dH/dp");

        plus = photon;
        minus = photon;
        plus.position[i] += eps;
        minus.position[i] -= eps;
        const double dHdx {(kerr::hamiltonian(plus, a) - kerr::hamiltonian(minus, a)) / (2.0 * eps)};
        expectNear(derivative.momentum[i], -dHdx, 1e-7, "dp/dlambda = -dH/dx");
    }
}

// Fate of a ray traced like the renderer's (past-directed, p_t = +1) that starts at x = -distance,
// y = b, z = 0 moving along +x. It is the time reverse of light travelling along -x at height b,
// which has L_z / E = +b: b > 0 is prograde, b < 0 retrograde.
[[nodiscard]] kerr::TraceResult incomingRay(const double b, const double a, const double distance = 1.0e4)
{
    const kerr::Photon photon {kerr::photonAlong({-distance, b, 0.0}, {1.0, 0.0, 0.0}, 1.0, a)};
    return kerr::traceRay(photon, a, kerr::TraceSettings {.escapeRadius = 2.0 * distance});
}

void testSchwarzschild()
{
    // Weak-field deflection to second order (e.g. Keeton & Petters 2005): 4/b + 15 pi / (4 b^2) + O(b^-3).
    constexpr double b = 100.0;
    const kerr::TraceResult result {incomingRay(b, 0.0, 1.0e5)};
    expect(result.fate == kerr::RayFate::ESCAPED, "b = 100 escapes");
    const auto [px, py, pz] {result.photon.momentum};
    const double deflection {std::atan2(-py, px)};
    expectNear(deflection, 4.0 / b + 15.0 * std::numbers::pi / (4.0 * b * b), 1e-4, "weak-field deflection");
    expectNear(kerr::hamiltonian(result.photon, 0.0), 0.0, 1e-9, "H conserved (weak field)");

    constexpr double bCritical = kerr::schwarzschildCriticalImpactParameter;
    expect(incomingRay(bCritical * 1.002, 0.0).fate == kerr::RayFate::ESCAPED, "b just above 3 sqrt 3 escapes");
    expect(incomingRay(bCritical * 0.998, 0.0).fate == kerr::RayFate::CAPTURED, "b just below 3 sqrt 3 is captured");

    // A ray that winds around the photon sphere keeps H ~ 0.
    const kerr::TraceResult winding {incomingRay(bCritical * 1.0001, 0.0)};
    expect(winding.fate == kerr::RayFate::ESCAPED, "near-critical ray escapes");
    expectNear(kerr::hamiltonian(winding.photon, 0.0), 0.0, 1e-6, "H conserved (near-critical ray)");
}

void testKerrCapture()
{
    constexpr double a = 0.9;
    // Critical impact parameters: b = xi at the prograde (b > 0) and retrograde (b < 0) photon orbits.
    const double xiPro {kerr::sphericalPhotonOrbit(kerr::photonOrbitRadius(a, kerr::Orbit::PROGRADE), a).xi};
    const double xiRetro {kerr::sphericalPhotonOrbit(kerr::photonOrbitRadius(a, kerr::Orbit::RETROGRADE), a).xi};
    expect(xiPro > 0.0 && xiRetro < 0.0, "xi > 0 prograde, < 0 retrograde");

    constexpr double margin = 0.005;
    expect(incomingRay(xiPro * (1.0 + margin), a).fate == kerr::RayFate::ESCAPED, "prograde ray outside escapes");
    expect(incomingRay(xiPro * (1.0 - margin), a).fate == kerr::RayFate::CAPTURED, "prograde ray inside captured");
    expect(incomingRay(xiRetro * (1.0 + margin), a).fate == kerr::RayFate::ESCAPED, "retrograde ray outside escapes");
    expect(incomingRay(xiRetro * (1.0 - margin), a).fate == kerr::RayFate::CAPTURED,
           "retrograde ray inside captured");
}

// Fate of the camera ray at angle psi to the right of forward, for a ZAMO camera on the +x axis
// looking at the hole with +z up.
[[nodiscard]] kerr::RayFate cameraRayFate(const double distance, const double psi, const double a)
{
    const kerr::ThreeVector position {distance, 0.0, 0.0};
    const kerr::Tetrad tetrad {kerr::zamoTetrad(position, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, {-1.0, 0.0, 0.0}, a)};
    const kerr::Photon ray {kerr::cameraRay(position, tetrad, {std::sin(psi), 0.0, std::cos(psi)}, a)};
    return kerr::traceRay(ray, a, kerr::TraceSettings {.escapeRadius = 4.0 * distance}).fate;
}

void testCamera()
{
    // Schwarzschild, static observer at r = 20: the exact shadow radius.
    constexpr double distance = 20.0;
    const double psi {std::asin(kerr::schwarzschildCriticalImpactParameter * std::sqrt(1.0 - 2.0 / distance)
                                / distance)};
    expect(cameraRayFate(distance, psi * 1.003, 0.0) == kerr::RayFate::ESCAPED, "a = 0: outside the shadow edge");
    expect(cameraRayFate(distance, psi * 0.997, 0.0) == kerr::RayFate::CAPTURED, "a = 0: inside the shadow edge");

    // Kerr, distant equatorial camera: the right edge is at alpha = -xi_retro, the left at -xi_pro.
    constexpr double a = 0.9;
    constexpr double far = 1000.0;
    const double xiPro {kerr::sphericalPhotonOrbit(kerr::photonOrbitRadius(a, kerr::Orbit::PROGRADE), a).xi};
    const double xiRetro {kerr::sphericalPhotonOrbit(kerr::photonOrbitRadius(a, kerr::Orbit::RETROGRADE), a).xi};
    const auto angle = [&](const double alpha) { return std::asin(alpha * std::sqrt(1.0 - 2.0 / far) / far); };
    constexpr double margin = 0.01;
    expect(cameraRayFate(far, angle(-xiRetro * (1.0 + margin)), a) == kerr::RayFate::ESCAPED, "right edge, outside");
    expect(cameraRayFate(far, angle(-xiRetro * (1.0 - margin)), a) == kerr::RayFate::CAPTURED, "right edge, inside");
    expect(cameraRayFate(far, angle(-xiPro * (1.0 + margin)), a) == kerr::RayFate::ESCAPED, "left edge, outside");
    expect(cameraRayFate(far, angle(-xiPro * (1.0 - margin)), a) == kerr::RayFate::CAPTURED, "left edge, inside");
}

// The renderer stops rays at r = max(200, 4 x camera distance) and looks the sky up in the ray's
// coordinate direction dx/dlambda. Outgoing Kerr-Schild coordinates are built on straight outgoing
// null lines, so that direction converges quickly: compare it with integrating to r = 1e6.
void testEscapeDirection()
{
    const auto unit = [](const kerr::ThreeVector& v) {
        const double length {std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])};
        return kerr::ThreeVector {v[0] / length, v[1] / length, v[2] / length};
    };
    for (const double a : {0.0, 0.9}) {
        for (const double b : {-12.0, 6.0, 8.0, 15.0, 40.0}) {
            const kerr::Photon start {kerr::photonAlong({-150.0, b, 7.0}, {1.0, 0.0, -0.05}, 1.0, a)};
            const kerr::TraceResult nearResult {kerr::traceRay(start, a, kerr::TraceSettings {.escapeRadius = 200.0})};
            const kerr::TraceResult farResult {kerr::traceRay(start, a, kerr::TraceSettings {.escapeRadius = 1.0e6})};
            expect(nearResult.fate == kerr::RayFate::ESCAPED && farResult.fate == kerr::RayFate::ESCAPED,
                   "rays escape");
            const kerr::ThreeVector nearDirection {unit(kerr::geodesicDerivative(nearResult.photon, a).position)};
            const kerr::ThreeVector farDirection {unit(kerr::geodesicDerivative(farResult.photon, a).position)};
            const double cosine {nearDirection[0] * farDirection[0] + nearDirection[1] * farDirection[1]
                                 + nearDirection[2] * farDirection[2]};
            expectNear(std::acos(std::min(cosine, 1.0)), 0.0, 1e-4, "direction at r = 200 is the asymptotic one");
        }
    }
}

void testShadowOutline()
{
    const auto circle {kerr::shadowOutline(0.0, 1.0, 64)};
    for (const auto& [alpha, beta] : circle) {
        expectNear(std::hypot(alpha, beta), kerr::schwarzschildCriticalImpactParameter, 1e-12, "a = 0 outline");
    }
    constexpr double a = 0.9;
    const auto outline {kerr::shadowOutline(a, std::numbers::pi / 2.0, 4096)};
    double left {0.0};
    double right {0.0};
    for (const auto& [alpha, beta] : outline) {
        left = std::min(left, alpha);
        right = std::max(right, alpha);
    }
    const double xiPro {kerr::sphericalPhotonOrbit(kerr::photonOrbitRadius(a, kerr::Orbit::PROGRADE), a).xi};
    const double xiRetro {kerr::sphericalPhotonOrbit(kerr::photonOrbitRadius(a, kerr::Orbit::RETROGRADE), a).xi};
    expectNear(left, -xiPro, 1e-6, "outline left edge");
    expectNear(right, -xiRetro, 1e-6, "outline right edge");
}

} // namespace

int main()
{
    testMetric();
    testZamo();
    testHamiltonEquations();
    testSchwarzschild();
    testKerrCapture();
    testCamera();
    testEscapeDirection();
    testShadowOutline();

    if (g_failures != 0) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all geodesic checks passed\n";
    return 0;
}
