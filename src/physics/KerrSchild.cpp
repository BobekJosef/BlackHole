#include "physics/KerrSchild.hpp"

#include <cmath>
#include <initializer_list>

namespace bh::kerr {

namespace {

struct KerrSchildField {
    double r {};
    double f {};
    FourVector l {}; // covariant l_mu
};

[[nodiscard]] KerrSchildField field(const ThreeVector& position, const double a)
{
    const auto [x, y, z] {position};
    const double r {boyerLindquistRadius(position, a)};
    const double r2 {r * r};
    const double s {r2 + a * a};
    return KerrSchildField {
        .r = r,
        .f = 2.0 * r2 * r / (r2 * r2 + a * a * z * z),
        .l = {1.0, -(r * x - a * y) / s, -(r * y + a * x) / s, -z / r},
    };
}

constexpr FourVector minkowskiDiagonal {-1.0, 1.0, 1.0, 1.0};

} // namespace

double boyerLindquistRadius(const ThreeVector& position, const double a)
{
    const auto [x, y, z] {position};
    const double b {x * x + y * y + z * z - a * a};
    return std::sqrt(0.5 * (b + std::sqrt(b * b + 4.0 * a * a * z * z)));
}

Metric metric(const ThreeVector& position, const double a)
{
    const auto [r, f, l] {field(position, a)};
    Metric g {};
    for (std::size_t mu {0}; mu < 4; ++mu) {
        for (std::size_t nu {0}; nu < 4; ++nu) {
            g[mu][nu] = (mu == nu ? minkowskiDiagonal[mu] : 0.0) + f * l[mu] * l[nu];
        }
    }
    return g;
}

Metric inverseMetric(const ThreeVector& position, const double a)
{
    const auto [r, f, l] {field(position, a)};
    const FourVector lUp {-l[0], l[1], l[2], l[3]}; // l^mu = eta^{mu nu} l_nu
    Metric g {};
    for (std::size_t mu {0}; mu < 4; ++mu) {
        for (std::size_t nu {0}; nu < 4; ++nu) {
            g[mu][nu] = (mu == nu ? minkowskiDiagonal[mu] : 0.0) - f * lUp[mu] * lUp[nu];
        }
    }
    return g;
}

FourVector lower(const Metric& g, const FourVector& v)
{
    FourVector result {};
    for (std::size_t mu {0}; mu < 4; ++mu) {
        for (std::size_t nu {0}; nu < 4; ++nu) {
            result[mu] += g[mu][nu] * v[nu];
        }
    }
    return result;
}

double dot(const Metric& g, const FourVector& u, const FourVector& v)
{
    const FourVector uLower {lower(g, u)};
    return uLower[0] * v[0] + uLower[1] * v[1] + uLower[2] * v[2] + uLower[3] * v[3];
}

Tetrad zamoTetrad(const ThreeVector& position, const ThreeVector& right, const ThreeVector& up,
                  const ThreeVector& forward, const double a)
{
    const Metric g {metric(position, a)};
    const Metric gInverse {inverseMetric(position, a)};

    // grad t_BL = grad t_KS + 2r/Delta grad r, with grad r from the implicit definition of r:
    // dr/dx = r^3 x / Q, dr/dy = r^3 y / Q, dr/dz = r z (r^2 + a^2) / Q, Q = r^4 + a^2 z^2.
    const auto [x, y, z] {position};
    const double r {boyerLindquistRadius(position, a)};
    const double r2 {r * r};
    const double q {r2 * r2 + a * a * z * z};
    const double delta {r2 - 2.0 * r + a * a};
    const double scale {2.0 * r / delta};
    const FourVector gradT {1.0, scale * r2 * r * x / q, scale * r2 * r * y / q, scale * r * z * (r2 + a * a) / q};

    // u^mu = -g^{mu nu} grad_nu t_BL / sqrt(-g^{ab} grad_a t grad_b t): future-directed, unit.
    FourVector u {};
    double norm {};
    for (std::size_t mu {0}; mu < 4; ++mu) {
        for (std::size_t nu {0}; nu < 4; ++nu) {
            u[mu] -= gInverse[mu][nu] * gradT[nu];
        }
        norm += u[mu] * gradT[mu]; // = -g^{mu nu} grad_mu t grad_nu t > 0 (grad t is timelike)
    }
    for (double& component : u) {
        component /= std::sqrt(norm);
    }

    // Gram-Schmidt in the observer's rest space (g(u, u) = -1, spatial vectors have g = +1).
    Tetrad tetrad {.time = u};
    const auto orthonormalise = [&](const ThreeVector& direction, const std::initializer_list<FourVector> done) {
        FourVector v {0.0, direction[0], direction[1], direction[2]};
        const double alongU {dot(g, v, u)};
        for (std::size_t mu {0}; mu < 4; ++mu) {
            v[mu] += alongU * u[mu];
        }
        for (const FourVector& e : done) {
            const double along {dot(g, v, e)};
            for (std::size_t mu {0}; mu < 4; ++mu) {
                v[mu] -= along * e[mu];
            }
        }
        const double length {std::sqrt(dot(g, v, v))};
        for (double& component : v) {
            component /= length;
        }
        return v;
    };
    tetrad.forward = orthonormalise(forward, {});
    tetrad.up = orthonormalise(up, {tetrad.forward});
    tetrad.right = orthonormalise(right, {tetrad.forward, tetrad.up});
    return tetrad;
}

double frameDraggingAngularVelocity(const double r, const double theta, const double a)
{
    const double r2a2 {r * r + a * a};
    const double delta {r * r - 2.0 * r + a * a};
    const double sinTheta {std::sin(theta)};
    return 2.0 * a * r / (r2a2 * r2a2 - a * a * delta * sinTheta * sinTheta);
}

} // namespace bh::kerr
