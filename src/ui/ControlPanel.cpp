#include "ui/ControlPanel.hpp"

#include "physics/Kerr.hpp"

#include <imgui.h>

#include <numbers>

namespace bh {

namespace {

void drawPhysicsReadout(const SceneSettings& settings)
{
    const double a {static_cast<double>(settings.spin)};
    ImGui::Text("Horizon r+       %.4f M", kerr::outerHorizonRadius(a));
    ImGui::Text("Ergosphere (eq.) %.4f M", kerr::ergosphereRadius(a, std::numbers::pi / 2.0));
    ImGui::Text("Photon orbit     %.4f M", kerr::photonOrbitRadius(a, settings.diskOrbit));
    ImGui::Text("ISCO             %.4f M", kerr::iscoRadius(a, settings.diskOrbit));
}

} // namespace

PanelActions drawControlPanel(SceneSettings& settings, const PanelInfo& info)
{
    PanelActions actions {};

    ImGui::SetNextWindowPos(ImVec2 {12.0F, 12.0F}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2 {340.0F, 0.0F}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Black hole")) {
        ImGui::End();
        return actions;
    }

    ImGui::TextDisabled("Stage 0: flat-space preview, no lensing yet");

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

    if (ImGui::CollapsingHeader("Camera & image", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Vertical FOV", &settings.fovDegrees, 10.0F, 140.0F, "%.0f deg");
        ImGui::SliderFloat("Exposure", &settings.exposure, 0.1F, 10.0F, "%.2f", ImGuiSliderFlags_Logarithmic);
        ImGui::Checkbox("VSync", &settings.vsync);
        ImGui::Text("Distance %.2f M", info.cameraDistance);
    }

    ImGui::Separator();
    ImGui::Text("%.1f FPS (%.2f ms)", info.fps, info.frameMilliseconds);
    ImGui::TextDisabled("%.*s", static_cast<int>(info.glRenderer.size()), info.glRenderer.data());
    actions.reloadShaders = ImGui::Button("Reload shaders (R)");
    if (!info.shaderError.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4 {1.0F, 0.4F, 0.4F, 1.0F});
        ImGui::TextWrapped("%.*s", static_cast<int>(info.shaderError.size()), info.shaderError.data());
        ImGui::PopStyleColor();
    }
    ImGui::TextDisabled("Drag: orbit  Scroll: zoom  Esc: quit");

    ImGui::End();
    return actions;
}

} // namespace bh
