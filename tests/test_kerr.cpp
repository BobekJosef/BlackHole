// Checks the closed-form Kerr quantities against textbook limits
// (Bardeen, Press & Teukolsky 1972).

#include "Check.hpp"
#include "physics/Kerr.hpp"

#include <cmath>
#include <numbers>

namespace {

using bh::test::expectNear;

} // namespace

TEST_CASE("Kerr closed forms against textbook limits")
{
    using bh::kerr::Orbit;
    namespace kerr = bh::kerr;

    expectNear(kerr::outerHorizonRadius(0.0), 2.0, 1e-9, "Schwarzschild horizon");
    expectNear(kerr::outerHorizonRadius(1.0), 1.0, 1e-9, "extremal horizon");
    expectNear(kerr::innerHorizonRadius(0.0), 0.0, 1e-9, "Schwarzschild inner horizon");
    expectNear(kerr::ergosphereRadius(0.9, std::numbers::pi / 2.0), 2.0, 1e-9, "equatorial ergosphere");
    expectNear(kerr::ergosphereRadius(0.9, 0.0), kerr::outerHorizonRadius(0.9), 1e-9, "ergosphere at pole");

    expectNear(kerr::iscoRadius(0.0, Orbit::PROGRADE), 6.0, 1e-9, "Schwarzschild ISCO (prograde)");
    expectNear(kerr::iscoRadius(0.0, Orbit::RETROGRADE), 6.0, 1e-9, "Schwarzschild ISCO (retrograde)");
    expectNear(kerr::iscoRadius(1.0, Orbit::PROGRADE), 1.0, 1e-9, "extremal prograde ISCO");
    expectNear(kerr::iscoRadius(1.0, Orbit::RETROGRADE), 9.0, 1e-9, "extremal retrograde ISCO");
    expectNear(kerr::iscoRadius(0.998, Orbit::PROGRADE), 1.2369, 1e-4, "Thorne-limit ISCO");

    expectNear(kerr::photonOrbitRadius(0.0, Orbit::PROGRADE), 3.0, 1e-9, "Schwarzschild photon orbit");
    expectNear(kerr::photonOrbitRadius(1.0, Orbit::PROGRADE), 1.0, 1e-9, "extremal prograde photon orbit");
    expectNear(kerr::photonOrbitRadius(1.0, Orbit::RETROGRADE), 4.0, 1e-9, "extremal retrograde photon orbit");

    expectNear(kerr::keplerianAngularVelocity(6.0, 0.0, Orbit::PROGRADE), 1.0 / std::pow(6.0, 1.5), 1e-9,
               "Keplerian Omega at r = 6");
    expectNear(kerr::schwarzschildCriticalImpactParameter, 3.0 * std::numbers::sqrt3, 1e-14,
               "critical impact parameter");
}
