#include "physics/Observer.hpp"

#include <algorithm>
#include <cmath>

namespace bh::kerr {

namespace {

// RK4 sub-step: a small fraction of the dynamical time r^1.5, shrunk near the horizon.
constexpr double stepPerDynamicalTime = 0.005;
constexpr double minHorizonFactor = 0.01;
// Launch speeds are clamped below c (gamma <= ~22).
constexpr double maxLaunchSpeed = 0.999;

} // namespace

std::optional<FourVector> keplerianVelocity(const ThreeVector& position, const double a, const Orbit orbit)
{
    const double r {boyerLindquistRadius(position, a)};
    if (r <= photonOrbitRadius(a, orbit)) {
        return std::nullopt; // circular motion at that speed would be faster than light
    }
    const double omega {keplerianAngularVelocity(r, a, orbit)};
    FourVector u {1.0, -omega * position[1], omega * position[0], 0.0};
    const double norm {-dot(metric(position, a), u, u)};
    if (!(norm > 0.0)) {
        return std::nullopt; // off the equator the same Omega can still be superluminal
    }
    for (double& component : u) {
        component /= std::sqrt(norm);
    }
    return u;
}

double speedRelativeToZamo(const ThreeVector& position, const FourVector& velocity, const double a)
{
    const double gamma {-dot(metric(position, a), velocity, zamoVelocity(position, a))};
    return std::sqrt(std::max(1.0 - 1.0 / (gamma * gamma), 0.0));
}

Photon launchedFromZamo(const ThreeVector& position, const double a, const double sidewaysSpeed)
{
    const Metric g {metric(position, a)};
    FourVector u {zamoVelocity(position, a)};
    const FourVector phi {0.0, -position[1], position[0], 0.0}; // d_phi
    const double phiNorm {std::sqrt(dot(g, phi, phi))};
    if (sidewaysSpeed != 0.0 && phiNorm > 0.0) {
        const double v {std::clamp(sidewaysSpeed, -maxLaunchSpeed, maxLaunchSpeed)};
        const double gamma {1.0 / std::sqrt(1.0 - v * v)};
        for (std::size_t mu {0}; mu < 4; ++mu) {
            u[mu] = gamma * (u[mu] + v * phi[mu] / phiNorm);
        }
    }
    const FourVector p {lower(g, u)};
    return Photon {.position = position, .momentum = {p[1], p[2], p[3]}, .pt = p[0]};
}

Photon fallingFromRest(const ThreeVector& position, const double a)
{
    return launchedFromZamo(position, a, 0.0);
}

ProperTimeStep advanceProperTime(Photon state, const double a, const double tau, const double stopRadius)
{
    const double horizon {outerHorizonRadius(a)};
    double elapsed {0.0};
    while (elapsed < tau) {
        const double r {boyerLindquistRadius(state.position, a)};
        if (r <= stopRadius) {
            break;
        }
        const double nearHorizon {std::max((r - horizon) / r, minHorizonFactor)};
        const double h {std::min(tau - elapsed, stepPerDynamicalTime * r * std::sqrt(r) * nearHorizon)};
        state = rungeKuttaStep(state, a, h);
        elapsed += h;
    }
    return ProperTimeStep {.state = state, .elapsed = elapsed};
}

FourVector observerVelocity(const Photon& state, const double a)
{
    const FourVector p {state.pt, state.momentum[0], state.momentum[1], state.momentum[2]};
    return raise(inverseMetric(state.position, a), p);
}

} // namespace bh::kerr
