// Moving observers (physics/Observer.hpp) and the camera frame built around them, against closed forms:
//  - the Keplerian observer on the equator is a circular geodesic (r stays, phi advances at Omega);
//    no circular motion inside the photon orbit; speed 1 / sqrt(r - 2) relative to the static observer
//    in Schwarzschild;
//  - aberration cos theta' = (cos theta + v) / (1 + v cos theta) between the moving camera and the ZAMO,
//    and the Doppler factor sqrt((1 + v) / (1 - v)) of the sky straight ahead: exact special relativity
//    at one event, so tight tolerances;
//  - radial free fall from rest in Schwarzschild follows the cycloid r = r0 (1 + cos eta) / 2,
//    tau = sqrt(r0^3 / 8) (eta + sin eta); in Kerr it keeps H = -1/2 and L_z = 0 down to the horizon.

#include "physics/Geodesic.hpp"
#include "physics/Kerr.hpp"
#include "physics/KerrSchild.hpp"
#include "physics/Observer.hpp"

#include <array>
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
        std::cerr << where.file_name() << ':' << where.line() << ": " << what << ": " << actual << " vs " << expected
                  << '\n';
    }
}

[[nodiscard]] kerr::FourVector combine(const double s, const kerr::FourVector& u, const double t,
                                       const kerr::FourVector& v)
{
    return {s * u[0] + t * v[0], s * u[1] + t * v[1], s * u[2] + t * v[2], s * u[3] + t * v[3]};
}

// Equatorial point at Boyer-Lindquist radius r on the +x side (x^2 + y^2 = r^2 + a^2 on the equator).
[[nodiscard]] kerr::ThreeVector equatorial(const double r, const double a)
{
    return {std::sqrt(r * r + a * a), 0.0, 0.0};
}

void testKeplerianObserver()
{
    for (const double a : {0.0, 0.9}) {
        for (const kerr::Orbit orbit : {kerr::Orbit::PROGRADE, kerr::Orbit::RETROGRADE}) {
            constexpr double r = 8.0;
            const kerr::ThreeVector position {equatorial(r, a)};
            const auto u {kerr::keplerianVelocity(position, a, orbit)};
            expect(u.has_value(), "circular orbit exists outside the photon orbit");
            if (!u) {
                continue;
            }
            expectNear(kerr::dot(kerr::metric(position, a), *u, *u), -1.0, 1e-12, "unit timelike");
            expect((*u)[0] > 0.0, "future-directed");

            // Integrate it as a timelike geodesic for a quarter of an orbit in coordinate time.
            const kerr::FourVector p {kerr::lower(kerr::metric(position, a), *u)};
            kerr::Photon state {.position = position, .momentum = {p[1], p[2], p[3]}, .pt = p[0]};
            const double omega {kerr::keplerianAngularVelocity(r, a, orbit)};
            const double quarter {0.5 * std::numbers::pi / std::abs(omega)}; // coordinate time
            state = kerr::advanceProperTime(state, a, quarter / (*u)[0], 0.0).state;
            expectNear(kerr::hamiltonian(state, a), -0.5, 1e-10, "H = -1/2 conserved");
            expectNear(kerr::boyerLindquistRadius(state.position, a), r, 1e-7, "circular: r stays");
            const double turned {std::atan2(state.position[1], state.position[0])
                                 - std::atan2(position[1], position[0])};
            expectNear(turned, std::copysign(0.5 * std::numbers::pi, omega), 1e-6, "phi advances at Omega");
        }
    }
    expect(!kerr::keplerianVelocity(equatorial(2.9, 0.0), 0.0, kerr::Orbit::PROGRADE), "none inside r = 3");
    expect(kerr::keplerianVelocity(equatorial(3.1, 0.0), 0.0, kerr::Orbit::PROGRADE).has_value(), "one at r = 3.1");

    const kerr::ThreeVector position {equatorial(10.0, 0.0)};
    const auto u {kerr::keplerianVelocity(position, 0.0, kerr::Orbit::PROGRADE)};
    expectNear(kerr::speedRelativeToZamo(position, u.value_or(kerr::FourVector {}), 0.0), 1.0 / std::sqrt(8.0), 1e-12,
               "Schwarzschild orbital speed 1 / sqrt(r - 2)");
}

// Aberration between the moving camera and the ZAMO at the same event, in the plane of motion.
void testAberration()
{
    constexpr double a = 0.9;
    const kerr::ThreeVector position {equatorial(6.0, a)};
    const kerr::Metric g {kerr::metric(position, a)};
    const kerr::FourVector u {kerr::keplerianVelocity(position, a, kerr::Orbit::PROGRADE).value()};
    const kerr::FourVector zamo {kerr::zamoVelocity(position, a)};
    const double gamma {-kerr::dot(g, u, zamo)};
    const double v {std::sqrt(1.0 - 1.0 / (gamma * gamma))};
    expectNear(v, kerr::speedRelativeToZamo(position, u, a), 1e-14, "speed");

    // Direction of motion: in the ZAMO frame along u's spatial part, in the camera frame opposite to the ZAMO's.
    const auto unit = [&g](kerr::FourVector w) {
        const double length {std::sqrt(kerr::dot(g, w, w))};
        for (double& component : w) {
            component /= length;
        }
        return w;
    };
    const kerr::FourVector motionZamo {unit(combine(1.0, u, -gamma, zamo))};   // u + (u . u_Z) u_Z
    const kerr::FourVector motionCamera {unit(combine(-1.0, zamo, gamma, u))}; // -(u_Z + (u_Z . u) u)

    const kerr::Tetrad camera {kerr::observerTetrad(position, u, {0.0, 0.0, 1.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, a)};
    for (const double angle : {0.0, 0.4, 1.2, 2.0, 2.9}) {
        // Pixel direction n' in the camera frame, at `angle` from forward (= +y, the direction of motion)
        // towards right; the ray is p = -u + n'.
        const kerr::FourVector n {combine(std::cos(angle), camera.forward, std::sin(angle), camera.right)};
        const kerr::FourVector p {combine(-1.0, u, 1.0, n)};
        const double cosCamera {kerr::dot(g, n, motionCamera)};
        // In the ZAMO frame p = c (-u_Z + n_Z): c = p . u_Z and the source lies along n_Z, cos = (p . motion) / c.
        const double cosZamo {kerr::dot(g, p, motionZamo) / kerr::dot(g, p, zamo)};
        expectNear(cosCamera, (cosZamo + v) / (1.0 + v * cosZamo), 1e-12, "aberration");
    }
}

// The sky ahead is blueshifted by the Doppler factor, behind redshifted (static frame: g = 1 / sqrt(1 - 2/r)).
void testDoppler()
{
    constexpr double r = 50.0;
    const kerr::ThreeVector position {equatorial(r, 0.0)};
    const kerr::FourVector u {kerr::keplerianVelocity(position, 0.0, kerr::Orbit::PROGRADE).value()};
    const double v {kerr::speedRelativeToZamo(position, u, 0.0)};
    const kerr::Tetrad camera {kerr::observerTetrad(position, u, {0.0, 0.0, 1.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, 0.0)};
    const double gravitational {1.0 / std::sqrt(1.0 - 2.0 / r)};
    const kerr::Photon ahead {kerr::cameraRay(position, camera, {0.0, 0.0, 1.0}, 0.0)};
    const kerr::Photon behind {kerr::cameraRay(position, camera, {0.0, 0.0, -1.0}, 0.0)};
    expectNear(kerr::skyRedshift(ahead) / gravitational, std::sqrt((1.0 + v) / (1.0 - v)), 1e-12, "blueshift ahead");
    expectNear(kerr::skyRedshift(behind) / gravitational, std::sqrt((1.0 - v) / (1.0 + v)), 1e-12, "redshift behind");
}

void testFreeFall()
{
    // Schwarzschild: the cycloid in proper time (independent of the coordinates).
    constexpr double r0 = 10.0;
    const kerr::Photon start {kerr::fallingFromRest({r0, 0.0, 0.0}, 0.0)};
    expectNear(kerr::hamiltonian(start, 0.0), -0.5, 1e-12, "unit 4-velocity");
    for (const double eta : {1.0, 2.0, 2.1}) { // r = 2 (the horizon) at eta = 2.21
        const double tau {std::sqrt(r0 * r0 * r0 / 8.0) * (eta + std::sin(eta))};
        const kerr::Photon state {kerr::advanceProperTime(start, 0.0, tau, 0.0).state};
        expectNear(kerr::boyerLindquistRadius(state.position, 0.0), 0.5 * r0 * (1.0 + std::cos(eta)), 1e-6,
                   "cycloid r(tau)");
        expectNear(kerr::hamiltonian(state, 0.0), -0.5, 1e-8, "H = -1/2 conserved");
    }

    // Kerr: zero angular momentum stays zero (frame dragging alone makes such a body orbit; that is a
    // statement in Boyer-Lindquist phi, which near the horizon differs from the Kerr-Schild azimuth here by
    // a coordinate twist, so it is checked through the ZAMO tests instead).
    constexpr double a = 0.9;
    const kerr::ThreeVector position {0.0, 10.0, 3.0};
    kerr::Photon state {kerr::fallingFromRest(position, a)};
    const auto angularMomentum = [](const kerr::Photon& s) {
        return s.position[0] * s.momentum[1] - s.position[1] * s.momentum[0];
    };
    expectNear(angularMomentum(state), 0.0, 1e-12, "dropped with L_z = 0");
    const double stop {1.2 * kerr::outerHorizonRadius(a)};
    int steps {0};
    while (kerr::boyerLindquistRadius(state.position, a) > stop && steps < 10000) {
        state = kerr::advanceProperTime(state, a, 0.5, stop).state;
        ++steps;
    }
    expect(kerr::boyerLindquistRadius(state.position, a) <= stop, "reaches the horizon in finite proper time");
    // The components grow as 1 / (r - r+) near the horizon: tolerances relative to that.
    expectNear(angularMomentum(state), 0.0, 1e-7, "L_z conserved");
    expectNear(kerr::hamiltonian(state, a), -0.5, 1e-7, "H = -1/2 conserved");

    // One step far longer than the fall stops at the stop radius, outside the horizon, without blowing up
    // (the app advances a whole frame at once; checking only between frames let the camera fall through
    // the horizon, where these coordinates are singular).
    const double horizon {kerr::outerHorizonRadius(a)};
    const kerr::ProperTimeStep step {kerr::advanceProperTime(kerr::fallingFromRest(position, a), a, 1.0e4, 1.05 * horizon)};
    const double r {kerr::boyerLindquistRadius(step.state.position, a)};
    expect(r > horizon && r <= 1.05 * horizon, "stops between r+ and the stop radius");
    expect(step.elapsed < 1.0e4, "reports the proper time actually advanced");
    expectNear(kerr::hamiltonian(step.state, a), -0.5, 1e-6, "still a unit 4-velocity there");
}

} // namespace

int main()
{
    testKeplerianObserver();
    testAberration();
    testDoppler();
    testFreeFall();

    if (g_failures != 0) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all observer checks passed\n";
    return 0;
}
