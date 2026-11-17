#include "physics/Blackbody.hpp"

#include <algorithm>
#include <cmath>

namespace bh {

namespace {

struct Xyz {
    double x {};
    double y {};
    double z {};
};

// Piecewise Gaussian with different widths left and right of the peak.
[[nodiscard]] double lobe(const double lambda, const double mean, const double sigmaLeft, const double sigmaRight)
{
    const double t {(lambda - mean) / (lambda < mean ? sigmaLeft : sigmaRight)};
    return std::exp(-0.5 * t * t);
}

// CIE 1931 2-degree colour matching functions, multi-lobe fit of Wyman, Sloan & Shirley,
// "Simple Analytic Approximations to the CIE XYZ Color Matching Functions", JCGT 2(2), 2013.
// Lambda in nm.
[[nodiscard]] Xyz colourMatching(const double lambda)
{
    return Xyz {
        .x = 1.056 * lobe(lambda, 599.8, 37.9, 31.0) + 0.362 * lobe(lambda, 442.0, 16.0, 26.7)
            - 0.065 * lobe(lambda, 501.1, 20.4, 26.2),
        .y = 0.821 * lobe(lambda, 568.8, 46.9, 40.5) + 0.286 * lobe(lambda, 530.9, 16.3, 31.1),
        .z = 1.217 * lobe(lambda, 437.0, 11.8, 36.0) + 0.681 * lobe(lambda, 459.0, 26.0, 13.8),
    };
}

// Planck spectral radiance B_lambda up to a constant: lambda^-5 / (exp(hc / (lambda k T)) - 1).
[[nodiscard]] double planck(const double lambdaNm, const double kelvin)
{
    constexpr double secondRadiationConstant = 1.438777e7; // hc/k in nm K
    const double l {lambdaNm * 1.0e-3};                    // in micrometres, to keep l^-5 moderate
    return 1.0 / (l * l * l * l * l * std::expm1(secondRadiationConstant / (lambdaNm * kelvin)));
}

[[nodiscard]] Xyz unnormalisedXyz(const double kelvin)
{
    constexpr double firstNm = 360.0;
    constexpr double lastNm = 830.0;
    constexpr int intervals = 470; // 1 nm
    Xyz sum {};
    for (int i {0}; i <= intervals; ++i) {
        const double lambda {firstNm + (lastNm - firstNm) * i / intervals};
        const double weight {(i == 0 || i == intervals) ? 0.5 : 1.0}; // trapezoidal rule
        const double radiance {weight * planck(lambda, kelvin)};
        const Xyz cmf {colourMatching(lambda)};
        sum.x += radiance * cmf.x;
        sum.y += radiance * cmf.y;
        sum.z += radiance * cmf.z;
    }
    return sum;
}

[[nodiscard]] Xyz normalisedXyz(const double kelvin)
{
    static const double scale {1.0 / unnormalisedXyz(6500.0).y};
    const Xyz xyz {unnormalisedXyz(kelvin)};
    return {xyz.x * scale, xyz.y * scale, xyz.z * scale};
}

} // namespace

LinearRgb blackbodyRgb(const double kelvin)
{
    const auto [x, y, z] {normalisedXyz(kelvin)};
    // XYZ -> linear sRGB (IEC 61966-2-1).
    return LinearRgb {
        .r = std::max(3.2406 * x - 1.5372 * y - 0.4986 * z, 0.0),
        .g = std::max(-0.9689 * x + 1.8758 * y + 0.0415 * z, 0.0),
        .b = std::max(0.0557 * x - 0.2040 * y + 1.0570 * z, 0.0),
    };
}

double blackbodyLuminance(const double kelvin)
{
    return normalisedXyz(kelvin).y;
}

} // namespace bh
