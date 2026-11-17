#include "physics/Kerr.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <ranges>

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

SphericalPhotonOrbit sphericalPhotonOrbit(const double r, const double a)
{
    const double a2 {a * a};
    const double rMinus1 {r - 1.0};
    const double rMinus3 {r - 3.0};
    return SphericalPhotonOrbit {
        .xi = (r * r * (3.0 - r) - a2 * (r + 1.0)) / (a * rMinus1),
        .eta = r * r * r * (4.0 * a2 - r * rMinus3 * rMinus3) / (a2 * rMinus1 * rMinus1),
    };
}

std::vector<ShadowPoint> shadowOutline(const double a, const double inclination, const std::size_t samples)
{
    std::vector<ShadowPoint> upper {};
    upper.reserve(samples);
    // Below this spin xi and eta lose all precision (they divide by a); the shadow is a circle.
    constexpr double minSpin = 1.0e-4;
    if (a < minSpin) {
        for (std::size_t i {0}; i < samples; ++i) {
            const double angle {std::numbers::pi * static_cast<double>(i) / static_cast<double>(samples - 1)};
            upper.push_back({-schwarzschildCriticalImpactParameter * std::cos(angle),
                             schwarzschildCriticalImpactParameter * std::sin(angle)});
        }
    } else {
        const double sinTheta {std::sin(inclination)};
        const double cosTheta {std::cos(inclination)};
        const double rPrograde {photonOrbitRadius(a, Orbit::PROGRADE)};
        const double rRetrograde {photonOrbitRadius(a, Orbit::RETROGRADE)};
        for (std::size_t i {0}; i < samples; ++i) {
            const double t {static_cast<double>(i) / static_cast<double>(samples - 1)};
            const auto [xi, eta] {sphericalPhotonOrbit(rPrograde + t * (rRetrograde - rPrograde), a)};
            const double beta2 {eta + a * a * cosTheta * cosTheta
                                - xi * xi * cosTheta * cosTheta / (sinTheta * sinTheta)};
            // Orbits that never reach the observer's latitude (beta^2 < 0) do not bound the shadow. The
            // tolerance keeps the equatorial orbits at the ends, where beta^2 = 0 up to rounding.
            constexpr double roundingTolerance = 1.0e-9;
            if (beta2 >= -roundingTolerance) {
                upper.push_back({-xi / sinTheta, std::sqrt(std::max(beta2, 0.0))});
            }
        }
    }

    std::vector<ShadowPoint> loop {upper};
    for (const ShadowPoint& point : upper | std::views::reverse) {
        loop.push_back({point.alpha, -point.beta});
    }
    return loop;
}

} // namespace bh::kerr
