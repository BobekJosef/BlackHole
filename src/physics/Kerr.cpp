#include "physics/Kerr.hpp"

#include <cmath>

namespace bh::kerr {

namespace {

// +1 for prograde, -1 for retrograde orbits.
[[nodiscard]] double orbitSign(const Orbit orbit)
{
    return orbit == Orbit::PROGRADE ? 1.0 : -1.0;
}

} // namespace

double outerHorizonRadius(const double a)
{
    return 1.0 + std::sqrt(1.0 - a * a);
}

double innerHorizonRadius(const double a)
{
    return 1.0 - std::sqrt(1.0 - a * a);
}

double ergosphereRadius(const double a, const double theta)
{
    const double cosTheta {std::cos(theta)};
    return 1.0 + std::sqrt(1.0 - a * a * cosTheta * cosTheta);
}

double iscoRadius(const double a, const Orbit orbit)
{
    const double z1 {1.0 + std::cbrt(1.0 - a * a) * (std::cbrt(1.0 + a) + std::cbrt(1.0 - a))};
    const double z2 {std::sqrt(3.0 * a * a + z1 * z1)};
    return 3.0 + z2 - orbitSign(orbit) * std::sqrt((3.0 - z1) * (3.0 + z1 + 2.0 * z2));
}

double photonOrbitRadius(const double a, const Orbit orbit)
{
    return 2.0 * (1.0 + std::cos(2.0 / 3.0 * std::acos(-orbitSign(orbit) * a)));
}

double keplerianAngularVelocity(const double r, const double a, const Orbit orbit)
{
    const double sign {orbitSign(orbit)};
    return sign / (r * std::sqrt(r) + sign * a);
}

} // namespace bh::kerr
