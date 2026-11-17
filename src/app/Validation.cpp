#include "app/Validation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <vector>

namespace bh {

namespace {

// Pass criteria. The two tracers agree to float rounding except where the geometry amplifies it: next to
// the photon ring a 1e-7 difference can decide between two ways around the hole, so a few pixels may
// differ in fate, and directions and radii have a far tail. Measured on an AMD iGPU (Mesa, 160 x 90): fate
// agreement 100 %; median direction difference 1e-7 rad (1e-5 in the face-on scene, whose few escaped
// rays all graze the photon ring); median disk radius difference 1e-7 relative. Limits 10-100x above.
constexpr double minFateAgreement = 0.995;
constexpr double maxMedianAngle = 1.0e-4;  // rad, escaped rays
constexpr double maxMedianRadius = 1.0e-5; // relative, disk hits

// trace.comp's FATE_* values.
[[nodiscard]] std::optional<kerr::RayFate> gpuFate(const float value)
{
    switch (static_cast<int>(std::lround(value))) {
    case 0: return kerr::RayFate::CAPTURED;
    case 1: return kerr::RayFate::ESCAPED;
    case 2: return kerr::RayFate::DISK;
    case 3: return kerr::RayFate::STEP_LIMIT;
    default: return std::nullopt;
    }
}

[[nodiscard]] std::size_t fateIndex(const kerr::RayFate fate)
{
    switch (fate) {
    case kerr::RayFate::CAPTURED: return 0;
    case kerr::RayFate::ESCAPED: return 1;
    case kerr::RayFate::DISK: return 2;
    case kerr::RayFate::STEP_LIMIT: return 3;
    }
    return 3;
}

// Median, 99th percentile and maximum of a sample (sorted in place).
struct Spread {
    double median {};
    double p99 {};
    double max {};
};

[[nodiscard]] Spread spread(std::vector<double>& values)
{
    if (values.empty()) {
        return {};
    }
    std::ranges::sort(values);
    const auto at = [&values](const double quantile) {
        return values[static_cast<std::size_t>(quantile * static_cast<double>(values.size() - 1))];
    };
    return Spread {.median = at(0.5), .p99 = at(0.99), .max = values.back()};
}

// Angle between two directions, accurate for tiny angles too (atan2 of |a x b| and a . b).
[[nodiscard]] double angleBetween(const kerr::ThreeVector& a, const glm::vec4& b)
{
    const std::array<double, 3> v {static_cast<double>(b.y), static_cast<double>(b.z), static_cast<double>(b.w)};
    const double cx {a[1] * v[2] - a[2] * v[1]};
    const double cy {a[2] * v[0] - a[0] * v[2]};
    const double cz {a[0] * v[1] - a[1] * v[0]};
    return std::atan2(std::sqrt(cx * cx + cy * cy + cz * cz), a[0] * v[0] + a[1] * v[1] + a[2] * v[2]);
}

[[nodiscard]] kerr::ThreeVector unit(const kerr::ThreeVector& v)
{
    const double length {std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])};
    return {v[0] / length, v[1] / length, v[2] / length};
}

} // namespace

ValidationResult compareWithReference(const std::string_view name, const ReferenceSetup& setup,
                                      const std::span<const glm::vec4> gpu)
{
    const std::size_t pixelCount {static_cast<std::size_t>(setup.width) * static_cast<std::size_t>(setup.height)};
    if (gpu.size() != pixelCount) {
        return ValidationResult {
            .passed = false,
            .report = std::format("{}: GPU returned {} pixels, expected {}", name, gpu.size(), pixelCount)};
    }

    std::array<std::size_t, 4> cpuFates {};
    std::size_t agree {0};
    std::vector<double> angles {};
    std::vector<double> radii {};
    for (int y {0}; y < setup.height; ++y) {
        for (int x {0}; x < setup.width; ++x) {
            const glm::vec4& gpuPixel {
                gpu[static_cast<std::size_t>(y) * static_cast<std::size_t>(setup.width) + static_cast<std::size_t>(x)]};
            const kerr::ThreeVector direction {
                kerr::cameraPixelDirection(x + 0.5, y + 0.5, setup.width, setup.height, setup.tanHalfFov)};
            const kerr::Photon ray {kerr::cameraRay(setup.cameraPosition, setup.cameraTetrad, direction, setup.spin)};
            const kerr::TraceResult cpu {kerr::traceRay(ray, setup.spin, setup.trace)};
            ++cpuFates[fateIndex(cpu.fate)];
            if (gpuFate(gpuPixel.x) != cpu.fate) {
                continue;
            }
            ++agree;
            if (cpu.fate == kerr::RayFate::ESCAPED) {
                const kerr::ThreeVector escape {unit(kerr::geodesicDerivative(cpu.photon, setup.spin).position)};
                angles.push_back(angleBetween(escape, gpuPixel));
            } else if (cpu.fate == kerr::RayFate::DISK && cpu.diskHit) {
                radii.push_back(std::abs(static_cast<double>(gpuPixel.w) - cpu.diskHit->radius) / cpu.diskHit->radius);
            }
        }
    }

    const double agreement {static_cast<double>(agree) / static_cast<double>(pixelCount)};
    const Spread angle {spread(angles)};
    const Spread radius {spread(radii)};
    const bool passed {agreement >= minFateAgreement && angle.median <= maxMedianAngle
                       && radius.median <= maxMedianRadius};
    ValidationResult result {.passed = passed, .report = {}};
    result.report += std::format("{} ({}x{}): {}\n", name, setup.width, setup.height, passed ? "PASS" : "FAIL");
    result.report +=
        std::format("  CPU fates: captured {}, escaped {}, disk {}, step limit {}; GPU agrees on "
                    "{:.3f} % (min {:.1f} %)\n",
                    cpuFates[0], cpuFates[1], cpuFates[2], cpuFates[3], 100.0 * agreement, 100.0 * minFateAgreement);
    result.report += std::format("  escape direction difference: median {:.2e}, 99 % {:.2e}, max {:.2e} rad "
                                 "(median <= {:.0e})\n",
                                 angle.median, angle.p99, angle.max, maxMedianAngle);
    result.report += std::format("  disk radius difference:      median {:.2e}, 99 % {:.2e}, max {:.2e} relative "
                                 "(median <= {:.0e})\n",
                                 radius.median, radius.p99, radius.max, maxMedianRadius);
    return result;
}

} // namespace bh
