#include "physics/Disk.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace bh::kerr {

namespace {

// The closed form divides by the root x2, which is 0 for a = 0 (its term vanishes in the limit).
constexpr double minAbsSpin = 1.0e-6;

[[nodiscard]] double signedSpin(const double a, const Orbit orbit)
{
    const double signedA {orbit == Orbit::PROGRADE ? a : -a};
    return std::abs(signedA) < minAbsSpin ? (orbit == Orbit::PROGRADE ? minAbsSpin : -minAbsSpin) : signedA;
}

} // namespace

double novikovThorneFlux(const double r, const double a, const Orbit orbit)
{
    const double rIsco {iscoRadius(a, orbit)};
    if (r <= rIsco) {
        return 0.0;
    }
    const double s {signedSpin(a, orbit)};
    const double x {std::sqrt(r)};
    const double x0 {std::sqrt(rIsco)};
    const double arc {std::acos(s)};
    const std::array roots {2.0 * std::cos((arc - std::numbers::pi) / 3.0),
                            2.0 * std::cos((arc + std::numbers::pi) / 3.0), -2.0 * std::cos(arc / 3.0)};

    double bracket {x - x0 - 1.5 * s * std::log(x / x0)};
    for (std::size_t i {0}; i < roots.size(); ++i) {
        const double xi {roots[i]};
        const double xj {roots[(i + 1) % 3]};
        const double xk {roots[(i + 2) % 3]};
        bracket -= 3.0 * (xi - s) * (xi - s) / (xi * (xi - xj) * (xi - xk)) * std::log((x - xi) / (x0 - xi));
    }
    const double x3 {x * x * x};
    return 3.0 / (8.0 * std::numbers::pi) * bracket / (x3 * x * (x3 - 3.0 * x + 2.0 * s));
}

double novikovThorneFluxPeakRadius(const double a, const Orbit orbit)
{
    const double rIsco {iscoRadius(a, orbit)};
    double low {rIsco};
    double high {4.0 * rIsco};
    const double invPhi {(std::sqrt(5.0) - 1.0) / 2.0};
    constexpr int iterations = 100;
    for (int i {0}; i < iterations; ++i) {
        const double m1 {high - invPhi * (high - low)};
        const double m2 {low + invPhi * (high - low)};
        if (novikovThorneFlux(m1, a, orbit) < novikovThorneFlux(m2, a, orbit)) {
            low = m1;
        } else {
            high = m2;
        }
    }
    return 0.5 * (low + high);
}

std::vector<double> diskTemperatureProfile(const double a, const Orbit orbit, const double rOut,
                                           const std::size_t samples)
{
    const double rIn {iscoRadius(a, orbit)};
    const double peakFlux {novikovThorneFlux(novikovThorneFluxPeakRadius(a, orbit), a, orbit)};
    std::vector<double> profile(samples);
    for (std::size_t i {0}; i < samples; ++i) {
        const double u {static_cast<double>(i) / static_cast<double>(samples - 1)};
        const double r {rIn + (rOut - rIn) * u * u};
        profile[i] = std::pow(std::max(novikovThorneFlux(r, a, orbit), 0.0) / peakFlux, 0.25);
    }
    return profile;
}

DiskRedshift diskRedshift(const double r, const double a, const Orbit orbit, const double pt,
                          const double angularMomentum)
{
    const double omega {keplerianAngularVelocity(r, a, orbit)};
    const double r2a2 {r * r + a * a};
    const double oneMinusAOmega {1.0 - a * omega};
    const double ut {1.0 / std::sqrt(1.0 - omega * omega * r2a2 - 2.0 / r * oneMinusAOmega * oneMinusAOmega)};
    const double nuEmitter {ut * (pt + omega * angularMomentum)};

    const double delta {r * r - 2.0 * r + a * a};
    const double bigA {r2a2 * r2a2 - a * a * delta};
    const double lapse {r * std::sqrt(delta / bigA)};
    const double frameDragging {2.0 * a * r / bigA};
    const double nuZamo {(pt + frameDragging * angularMomentum) / lapse};
    return DiskRedshift {.gravitational = 1.0 / nuZamo, .doppler = nuZamo / nuEmitter};
}

} // namespace bh::kerr
