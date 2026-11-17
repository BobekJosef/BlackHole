#include "ui/ControlPanel.hpp"

#include "physics/Kerr.hpp"

#include <imgui.h>

#include <numbers>
#include <utility>

namespace bh {

namespace {

constexpr std::array<const char*, 4> toneMapperNames {"AgX", "ACES (Hill fit)", "Exponential", "Clamp (none)"};

void drawPhysicsReadout(const SceneSettings& settings)
{
    const double a {static_cast<double>(settings.spin)};
    ImGui::Text("Horizon r+       %.4f M", kerr::outerHorizonRadius(a));
    ImGui::Text("Ergosphere (eq.) %.4f M", kerr::ergosphereRadius(a, std::numbers::pi / 2.0));
    ImGui::Text("Photon orbit     %.4f M", kerr::photonOrbitRadius(a, settings.diskOrbit));
    ImGui::Text("ISCO             %.4f M", kerr::iscoRadius(a, settings.diskOrbit));
}

void drawImageSettings(SceneSettings& settings, const PanelInfo& info)
{
    ImGui::SliderFloat("Render scale", &settings.renderScale, 0.25F, 1.0F, "%.2f");
    ImGui::SameLine();
    ImGui::TextDisabled("%dx%d", info.traceResolution.x, info.traceResolution.y);

    int toneMapper {std::to_underlying(settings.toneMapper)};
    if (ImGui::Combo("Tone mapper", &toneMapper, toneMapperNames.data(), static_cast<int>(toneMapperNames.size()))) {
        settings.toneMapper = static_cast<ToneMapper>(toneMapper);
    }
    ImGui::SliderFloat("Exposure", &settings.exposureEv, -6.0F, 6.0F, "%+.1f EV");
    ImGui::Checkbox("Bloom", &settings.bloomEnabled);
    ImGui::BeginDisabled(!settings.bloomEnabled);
    ImGui::SliderFloat("Bloom strength", &settings.bloomStrength, 0.0F, 0.3F, "%.3f");
    ImGui::EndDisabled();
}

void drawPerformance(SceneSettings& settings, const PanelInfo& info)
{
    ImGui::Text("CPU frame %.2f ms (%.0f FPS)", info.frameMilliseconds, info.fps);
    double gpuTotal {0.0};
    for (std::size_t pass {0}; pass < gpuPassNames.size(); ++pass) {
        const std::string_view name {gpuPassNames[pass]};
        ImGui::Text("GPU %-8.*s %6.3f ms", static_cast<int>(name.size()), name.data(), info.gpuMilliseconds[pass]);
        gpuTotal += info.gpuMilliseconds[pass];
    }
    ImGui::Text("GPU total    %6.3f ms", gpuTotal);
    ImGui::Checkbox("VSync", &settings.vsync);
    ImGui::SameLine();
    ImGui::TextDisabled("(off to measure)");
    ImGui::TextDisabled("%.*s", static_cast<int>(info.glRenderer.size()), info.glRenderer.data());
}

} // namespace

PanelActions drawControlPanel(SceneSettings& settings, const PanelInfo& info)
{
    PanelActions actions {};

    ImGui::SetNextWindowPos(ImVec2 {12.0F, 12.0F}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2 {360.0F, 0.0F}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Black hole")) {
        ImGui::End();
        return actions;
    }

    ImGui::TextDisabled("Flat-space preview, no lensing yet");
    if (!info.shaderError.empty()) { // on top, so it cannot scroll out of sight
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4 {1.0F, 0.4F, 0.4F, 1.0F});
        ImGui::TextWrapped("Shader error (previous shaders still active):\n%.*s",
                           static_cast<int>(info.shaderError.size()), info.shaderError.data());
        ImGui::PopStyleColor();
    }

    if (ImGui::CollapsingHeader("Black hole", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Spin a/M", &settings.spin, 0.0F, 0.998F, "%.3f");
        bool retrograde {settings.diskOrbit == kerr::Orbit::RETROGRADE};
        if (ImGui::Checkbox("Retrograde disk", &retrograde)) {
            settings.diskOrbit = retrograde ? kerr::Orbit::RETROGRADE : kerr::Orbit::PROGRADE;
        }
        drawPhysicsReadout(settings);
    }

    if (ImGui::CollapsingHeader("Accretion disk", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Show disk", &settings.diskEnabled);
        ImGui::SliderFloat("Outer radius", &settings.diskOuterRadius, 10.0F, 60.0F, "%.1f M");
    }

    if (ImGui::CollapsingHeader("Background", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Procedural stars", &settings.starsEnabled);
    }

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Vertical FOV", &settings.fovDegrees, 10.0F, 140.0F, "%.0f deg");
        ImGui::Text("Distance %.2f M", info.cameraDistance);
    }

    if (ImGui::CollapsingHeader("Image", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawImageSettings(settings, info);
    }

    if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawPerformance(settings, info);
    }

    if (ImGui::CollapsingHeader("Shaders")) {
        actions.reloadShaders = ImGui::Button("Reload (R)");
        ImGui::SameLine();
        ImGui::Checkbox("Reload on save", &settings.autoReloadShaders);
    }

    ImGui::Separator();
    ImGui::Text("%.0f FPS", info.fps);
    ImGui::SameLine();
    ImGui::TextDisabled("Drag: orbit  Scroll: zoom  Esc: quit");

    ImGui::End();
    return actions;
}

} // namespace bh
