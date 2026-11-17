#include "ui/ShadowOverlay.hpp"

#include <imgui.h>

#include <cmath>

namespace bh {

void drawShadowOutline(const std::vector<kerr::ShadowPoint>& outline, const double cameraRadius,
                       const double tanHalfFov)
{
    const ImVec2 display {ImGui::GetIO().DisplaySize};
    const double halfHeight {0.5 * static_cast<double>(display.y)};
    const double lapse {std::sqrt(1.0 - 2.0 / cameraRadius)};

    std::vector<ImVec2> points {};
    points.reserve(outline.size());
    for (const auto& [alpha, beta] : outline) {
        const double b {std::hypot(alpha, beta)};
        const double sinPsi {b * lapse / cameraRadius};
        if (b <= 0.0 || sinPsi >= 1.0) {
            continue; // camera too close for this point to be in front of it
        }
        // Pinhole projection: screen offset proportional to tan(psi), as in trace.comp.
        const double scale {std::tan(std::asin(sinPsi)) / tanHalfFov * halfHeight / b};
        points.emplace_back(static_cast<float>(0.5 * static_cast<double>(display.x) + alpha * scale),
                            static_cast<float>(halfHeight - beta * scale));
    }
    if (points.size() > 1) {
        ImGui::GetBackgroundDrawList()->AddPolyline(points.data(), static_cast<int>(points.size()),
                                                    IM_COL32(80, 255, 120, 220), ImDrawFlags_Closed, 1.5F);
    }
}

} // namespace bh
