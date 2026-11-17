// Checks the closed-form Kerr quantities against textbook limits
// (Bardeen, Press & Teukolsky 1972). Plain CTest executable: non-zero exit on failure.

#include "physics/Kerr.hpp"

#include <cmath>
#include <iostream>
#include <numbers>
#include <source_location>
#include <string_view>

namespace {

int g_failures {0};

void expectNear(const double actual, const double expected, const std::string_view what,
                const double tolerance = 1e-9,
                const std::source_location where = std::source_location::current())
{
    if (std::abs(actual - expected) <= tolerance) {
        return;
    }
    ++g_failures;
    std::cerr << where.file_name() << ':' << where.line() << ": " << what << ": got " << actual
              << ", expected " << expected << '\n';
}

} // namespace

int main()
{
    using bh::kerr::Orbit;
    namespace kerr = bh::kerr;

    expectNear(kerr::outerHorizonRadius(0.0), 2.0, "Schwarzschild horizon");
    expectNear(kerr::outerHorizonRadius(1.0), 1.0, "extremal horizon");
    expectNear(kerr::innerHorizonRadius(0.0), 0.0, "Schwarzschild inner horizon");
    expectNear(kerr::ergosphereRadius(0.9, std::numbers::pi / 2.0), 2.0, "equatorial ergosphere");
    expectNear(kerr::ergosphereRadius(0.9, 0.0), kerr::outerHorizonRadius(0.9), "ergosphere at pole");

    expectNear(kerr::iscoRadius(0.0, Orbit::PROGRADE), 6.0, "Schwarzschild ISCO (prograde)");
    expectNear(kerr::iscoRadius(0.0, Orbit::RETROGRADE), 6.0, "Schwarzschild ISCO (retrograde)");
    expectNear(kerr::iscoRadius(1.0, Orbit::PROGRADE), 1.0, "extremal prograde ISCO");
    expectNear(kerr::iscoRadius(1.0, Orbit::RETROGRADE), 9.0, "extremal retrograde ISCO");
    expectNear(kerr::iscoRadius(0.998, Orbit::PROGRADE), 1.2369, "Thorne-limit ISCO", 1e-4);

    expectNear(kerr::photonOrbitRadius(0.0, Orbit::PROGRADE), 3.0, "Schwarzschild photon orbit");
    expectNear(kerr::photonOrbitRadius(1.0, Orbit::PROGRADE), 1.0, "extremal prograde photon orbit");
    expectNear(kerr::photonOrbitRadius(1.0, Orbit::RETROGRADE), 4.0, "extremal retrograde photon orbit");

    expectNear(kerr::keplerianAngularVelocity(6.0, 0.0, Orbit::PROGRADE), 1.0 / std::pow(6.0, 1.5),
               "Keplerian Omega at r = 6");
    expectNear(kerr::schwarzschildCriticalImpactParameter, 3.0 * std::sqrt(3.0), "critical impact parameter",
               1e-14);

    if (g_failures != 0) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all Kerr checks passed\n";
    return 0;
}
