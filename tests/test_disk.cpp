// Validates the thin-disk emission and the black-body colours:
//  - the closed-form Page-Thorne flux against a direct numerical evaluation of the Page-Thorne
//    integral, F = -Omega' / (4 pi r (E - Omega L)^2) int_{r_ISCO}^r (E - Omega L) L' dr, built from the
//    circular-orbit E, L and Omega (Bardeen, Press & Teukolsky 1972) — two independent formulas;
//  - zero torque at the ISCO, the Newtonian limit far out, the flux peak;
//  - black-body colours: near white at 6500 K, red when cool, blue when hot, Rayleigh-Jeans
//    luminance ~T at high temperature.

#include "physics/Blackbody.hpp"
#include "physics/Disk.hpp"
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

void expectRelative(const double actual, const double expected, const double tolerance, const std::string_view what,
                    const std::source_location where = std::source_location::current())
{
    if (!(std::abs(actual - expected) <= tolerance * std::abs(expected))) { // NaN fails
        ++g_failures;
        std::cerr << where.file_name() << ':' << where.line() << ": " << what << ": got " << actual << ", expected "
                  << expected << " (relative tolerance " << tolerance << ")\n";
    }
}

// Specific energy, angular momentum and angular velocity of a prograde circular equatorial
// geodesic for signed spin s (retrograde orbits are s < 0), BPT 1972 eq. 2.12-2.13, M = 1.
struct CircularOrbit {
    double energy {};
    double angularMomentum {};
    double omega {};
};

[[nodiscard]] CircularOrbit circularOrbit(const double r, const double s)
{
    const double sqrtR {std::sqrt(r)};
    const double denominator {std::pow(r, 0.75) * std::sqrt(r * sqrtR - 3.0 * sqrtR + 2.0 * s)};
    return CircularOrbit {
        .energy = (r * sqrtR - 2.0 * sqrtR + s) / denominator,
        .angularMomentum = (r * r - 2.0 * s * sqrtR + s * s) / denominator,
        .omega = 1.0 / (r * sqrtR + s),
    };
}

// The Page-Thorne flux by direct numerical integration (Simpson), sqrt(-g) = r on the equator.
[[nodiscard]] double pageThorneIntegral(const double r, const double s, const double rIsco)
{
    const auto derivative = [](const auto& function, const double x) {
        const double h {1.0e-5 * x};
        return (function(x + h) - function(x - h)) / (2.0 * h);
    };
    const auto angularMomentum = [s](const double x) { return circularOrbit(x, s).angularMomentum; };
    const auto omega = [s](const double x) { return circularOrbit(x, s).omega; };
    const auto integrand = [&](const double x) {
        const CircularOrbit orbit {circularOrbit(x, s)};
        return (orbit.energy - orbit.omega * orbit.angularMomentum) * derivative(angularMomentum, x);
    };

    constexpr int intervals = 4000; // even
    const double h {(r - rIsco) / intervals};
    double integral {integrand(rIsco) + integrand(r)};
    for (int i {1}; i < intervals; ++i) {
        integral += (i % 2 == 1 ? 4.0 : 2.0) * integrand(rIsco + i * h);
    }
    integral *= h / 3.0;

    const CircularOrbit orbit {circularOrbit(r, s)};
    const double eMinusOmegaL {orbit.energy - orbit.omega * orbit.angularMomentum};
    return -derivative(omega, r) / (4.0 * std::numbers::pi * r * eMinusOmegaL * eMinusOmegaL) * integral;
}

void testNovikovThorne()
{
    struct Case {
        double a {};
        kerr::Orbit orbit {};
    };
    for (const auto& [a, orbit] : {Case {0.0, kerr::Orbit::PROGRADE}, Case {0.5, kerr::Orbit::PROGRADE},
                                   Case {0.9, kerr::Orbit::PROGRADE}, Case {0.998, kerr::Orbit::PROGRADE},
                                   Case {0.9, kerr::Orbit::RETROGRADE}}) {
        const double rIsco {kerr::iscoRadius(a, orbit)};
        const double s {orbit == kerr::Orbit::PROGRADE ? a : -a};
        expect(kerr::novikovThorneFlux(rIsco, a, orbit) == 0.0, "zero torque: F(r_ISCO) = 0");
        for (const double factor : {1.05, 1.5, 3.0, 10.0}) {
            const double r {factor * rIsco};
            expectRelative(kerr::novikovThorneFlux(r, a, orbit), pageThorneIntegral(r, s, rIsco), 1e-4,
                           "closed form = Page-Thorne integral");
        }
        const double rPeak {kerr::novikovThorneFluxPeakRadius(a, orbit)};
        const double peak {kerr::novikovThorneFlux(rPeak, a, orbit)};
        expect(peak > kerr::novikovThorneFlux(0.99 * rPeak, a, orbit)
                   && peak > kerr::novikovThorneFlux(1.01 * rPeak, a, orbit),
               "peak radius is a maximum");

        const auto profile {kerr::diskTemperatureProfile(a, orbit, 30.0, 256)};
        expect(profile.front() == 0.0 && *std::ranges::max_element(profile) <= 1.0 + 1e-12
                   && *std::ranges::max_element(profile) > 0.99,
               "temperature profile: 0 at the ISCO, peak 1");
    }

    // Newtonian limit far out: F -> 3 / (8 pi r^3) (1 - sqrt(r_ISCO / r)) up to O(ln r / sqrt r).
    constexpr double far = 1.0e8;
    const double newtonian {3.0 / (8.0 * std::numbers::pi * far * far * far) * (1.0 - std::sqrt(6.0 / far))};
    expectRelative(kerr::novikovThorneFlux(far, 0.0, kerr::Orbit::PROGRADE), newtonian, 1e-3, "Newtonian limit");
}

void testRedshift()
{
    // Face-on observer far away (L_z = 0) of a Schwarzschild disk: g = sqrt(1 - 3/r), transverse
    // Doppler and gravitational redshift together.
    for (const double r : {6.0, 10.0, 50.0}) {
        expectRelative(kerr::diskRedshift(r, 0.0, kerr::Orbit::PROGRADE, 1.0, 0.0).total(), std::sqrt(1.0 - 3.0 / r),
                       1e-12, "face-on g = sqrt(1 - 3/r)");
    }

    // Explicit contraction in Kerr-Schild coordinates: gas u = u^t (1, -Omega y, Omega x, 0) normalised
    // with the metric, the emitter's ZAMO from zamoTetrad, any null p at the emitter.
    struct Case {
        double a {};
        kerr::Orbit orbit {};
    };
    for (const auto& [a, orbit] : {Case {0.9, kerr::Orbit::PROGRADE}, Case {0.9, kerr::Orbit::RETROGRADE},
                                   Case {0.3, kerr::Orbit::PROGRADE}}) {
        const double r {1.3 * kerr::iscoRadius(a, orbit)};
        const double rho {std::sqrt(r * r + a * a)}; // on the equator x^2 + y^2 = r^2 + a^2
        const kerr::ThreeVector position {rho * std::cos(0.7), rho * std::sin(0.7), 0.0};
        const kerr::Metric g {kerr::metric(position, a)};
        const double omega {kerr::keplerianAngularVelocity(r, a, orbit)};
        kerr::FourVector u {1.0, -omega * position[1], omega * position[0], 0.0};
        const double norm {std::sqrt(-kerr::dot(g, u, u))};
        for (double& component : u) {
            component /= norm;
        }
        const kerr::Tetrad zamo {kerr::zamoTetrad(position, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, a)};

        for (const kerr::ThreeVector& direction :
             {kerr::ThreeVector {0.3, -0.8, 0.5}, kerr::ThreeVector {-0.6, 0.2, -0.7}, kerr::ThreeVector {0.0, 0.0, 1.0}}) {
            const kerr::Photon photon {kerr::photonAlong(position, direction, 1.3, a)};
            const kerr::FourVector p {photon.pt, photon.momentum[0], photon.momentum[1], photon.momentum[2]};
            const auto contract = [&p](const kerr::FourVector& v) {
                return p[0] * v[0] + p[1] * v[1] + p[2] * v[2] + p[3] * v[3];
            };
            const double angularMomentum {position[0] * photon.momentum[1] - position[1] * photon.momentum[0]};
            const kerr::DiskRedshift shift {kerr::diskRedshift(r, a, orbit, photon.pt, angularMomentum)};
            expectRelative(shift.total(), 1.0 / contract(u), 1e-10, "g = 1 / (p . u_gas)");
            expectRelative(shift.gravitational, 1.0 / contract(zamo.time), 1e-10, "g_grav = 1 / (p . u_ZAMO)");
        }
    }
}

void testBlackbody()
{
    expectRelative(bh::blackbodyLuminance(6500.0), 1.0, 1e-12, "normalisation Y(6500 K) = 1");

    const auto [r65, g65, b65] {bh::blackbodyRgb(6500.0)};
    expect(std::max({r65, g65, b65}) / std::min({r65, g65, b65}) < 1.15, "6500 K is near white");

    const auto [r3, g3, b3] {bh::blackbodyRgb(3000.0)};
    expect(r3 > g3 && g3 > b3, "3000 K is orange-red");
    const auto [r15, g15, b15] {bh::blackbodyRgb(15000.0)};
    expect(b15 > g15 && g15 > r15, "15000 K is blue-white");

    // Rayleigh-Jeans: in the visible band B ~ T once hc / (lambda k T) << 1.
    expectRelative(bh::blackbodyLuminance(2.0e6) / bh::blackbodyLuminance(1.0e6), 2.0, 0.01, "Y ~ T when hot");
    expect(bh::blackbodyLuminance(1000.0) < 1e-3 * bh::blackbodyLuminance(3000.0), "Wien cutoff when cool");
}

} // namespace

int main()
{
    testNovikovThorne();
    testRedshift();
    testBlackbody();

    if (g_failures != 0) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all disk checks passed\n";
    return 0;
}
