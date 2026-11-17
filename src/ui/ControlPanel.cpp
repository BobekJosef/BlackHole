#include "ui/ControlPanel.hpp"

#include "physics/Disk.hpp"
#include "physics/Kerr.hpp"
#include "render/BlackHoleRenderer.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <cfloat>
#include <numbers>
#include <utility>

namespace bh {

namespace {

constexpr std::array<const char*, 4> toneMapperNames {"AgX", "ACES (Hill fit)", "Exponential", "Clamp (none)"};
constexpr std::array<const char*, 6> debugViewNames {"None", "Step count", "Constraint |H| / p^2",
                                                     "Fate (captured/sky/disk/limit)", "Redshift g (disk)",
                                                     "Magnification (sky)"};
constexpr float panelWidth = 360.0F;   // at the default font
constexpr float panelFontSize = 13.0F; // Dear ImGui's default font size

// Indexed by ScreenshotSize.
constexpr std::array<const char*, 5> screenshotSizeNames {"Window", "1920 x 1080", "2560 x 1440", "3840 x 2160 (4K)",
                                                          "7680 x 4320 (8K)"};
// Indexed by Observer.
constexpr std::array<const char*, 3> observerNames {"Static (ZAMO)", "Orbiting (Keplerian)", "Free fall"};
// Indexed by Background.
constexpr std::array<const char*, 4> backgroundNames {"None (black)", "Stars (procedural)", "Coordinate grid",
                                                      "Image map"};

void drawRayTracingSettings(SceneSettings& settings)
{
    ImGui::SliderFloat("Step scale", &settings.stepScale, 0.005F, 0.1F, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SetItemTooltip("Affine step near the hole: h = scale * r^1.5. Smaller is more accurate and slower.");
    ImGui::SliderFloat("Far step limit", &settings.maxStepFraction, 0.05F, 0.5F, "%.2f r");
    ImGui::SliderInt("Max steps", &settings.maxSteps, 50, 4000, "%d", ImGuiSliderFlags_Logarithmic);
    int debugView {std::to_underlying(settings.debugView)};
    if (ImGui::Combo("Debug view", &debugView, debugViewNames.data(), static_cast<int>(debugViewNames.size()))) {
        settings.debugView = static_cast<DebugView>(debugView);
    }
    ImGui::Checkbox("Analytic shadow edge", &settings.shadowOutline);
    ImGui::SetItemTooltip("Bardeen (1973) critical curve; exact for distant observers.");
}

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
    ImGui::Checkbox("Fullscreen (F11)", &settings.fullscreen);
    ImGui::SliderFloat("Render scale", &settings.renderScale, 0.25F, 1.0F, "%.2f");
    ImGui::SameLine();
    ImGui::TextDisabled("%dx%d", info.traceResolution.x, info.traceResolution.y);

    int toneMapper {std::to_underlying(settings.toneMapper)};
    if (ImGui::Combo("Tone mapper", &toneMapper, toneMapperNames.data(), static_cast<int>(toneMapperNames.size()))) {
        settings.toneMapper = static_cast<ToneMapper>(toneMapper);
    }
    ImGui::Checkbox("Refine still image", &settings.refineStill);
    ImGui::SetItemTooltip("While nothing changes, average frames traced with sub-pixel offsets into an\n"
                          "anti-aliased image (then stop tracing). Disk turbulence animates, so set the\n"
                          "Time scale to 0 (or the turbulence to 0) for a still.");
    ImGui::SameLine();
    ImGui::TextDisabled("%u / %u", info.accumulatedSamples, BlackHoleRenderer::maxAccumulatedSamples);
    ImGui::Checkbox("Auto exposure", &settings.autoExposure);
    ImGui::SetItemTooltip("Meter the image (weighted by brightness) and adapt like a camera; Exposure then\n"
                          "adds to it. Not physical, but it keeps a blueshifted view from saturating.");
    ImGui::SliderFloat(settings.autoExposure ? "Exposure comp." : "Exposure", &settings.exposureEv, -6.0F, 6.0F,
                       "%+.1f EV");
    ImGui::Checkbox("Bloom", &settings.bloomEnabled);
    ImGui::BeginDisabled(!settings.bloomEnabled);
    ImGui::SliderFloat("Bloom strength", &settings.bloomStrength, 0.0F, 0.3F, "%.3f");
    ImGui::EndDisabled();
}


} // namespace

void ControlPanel::drawPerformance(SceneSettings& settings, const PanelInfo& info)
{
    ImGui::Text("CPU frame %.2f ms (%.0f FPS)", info.frameMilliseconds, info.fps);
    double gpuTotal {0.0};
    for (std::size_t pass {0}; pass < gpuPassNames.size(); ++pass) {
        const std::string_view name {gpuPassNames[pass]};
        ImGui::Text("GPU %-8.*s %6.3f ms", static_cast<int>(name.size()), name.data(), info.gpuMilliseconds[pass]);
        gpuTotal += info.gpuMilliseconds[pass];
    }
    ImGui::Text("GPU total    %6.3f ms", gpuTotal);
    // Applied on release: rescaling while dragging would move the slider under the cursor.
    if (!m_editingUiScale) {
        m_uiScaleDraft = settings.uiScale;
    }
    ImGui::SliderFloat("UI scale", &m_uiScaleDraft, 0.75F, 3.0F, "%.2f");
    m_editingUiScale = ImGui::IsItemActive();
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        settings.uiScale = m_uiScaleDraft;
    }
    ImGui::SetItemTooltip("Size of this panel, on top of the monitor's own scale.");
    ImGui::Checkbox("VSync", &settings.vsync);
    ImGui::SameLine();
    ImGui::TextDisabled("(off to measure)");
    ImGui::TextDisabled("%.*s", static_cast<int>(info.glRenderer.size()), info.glRenderer.data());
}

void ControlPanel::drawPresets(const PanelInfo& info, PanelActions& actions)
{
    if (ImGui::BeginCombo("Load", "(choose)")) {
        for (const std::filesystem::path& preset : info.presets) {
            const std::string name {preset.stem().string()};
            if (ImGui::Selectable(name.c_str())) {
                actions.loadPreset = preset;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    actions.rescanPresets = ImGui::SmallButton("Rescan##presets");
    const bool enter {ImGui::InputTextWithHint("##name", "name", &m_presetName, ImGuiInputTextFlags_EnterReturnsTrue)};
    ImGui::SameLine();
    if (ImGui::Button("Save") || enter) {
        actions.savePreset = m_presetName;
    }
    ImGui::SetItemTooltip("Saves the scene and camera to presets/<name>.preset (a short text file;\n"
                          "keys left out of a hand-written one take their defaults).");
    if (!info.presetStatus.empty()) {
        const ImVec4 color {info.presetError ? ImVec4 {1.0F, 0.4F, 0.4F, 1.0F}
                                              : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)};
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextWrapped("%.*s", static_cast<int>(info.presetStatus.size()), info.presetStatus.data());
        ImGui::PopStyleColor();
    }
}

void ControlPanel::drawCamera(SceneSettings& settings, const PanelInfo& info, PanelActions& actions)
{
    int observer {std::to_underlying(settings.observer)};
    if (ImGui::Combo("Observer", &observer, observerNames.data(), static_cast<int>(observerNames.size()))) {
        settings.observer = static_cast<Observer>(observer);
    }
    ImGui::SetItemTooltip("Static: at rest relative to the local frame dragging (ZAMO).\n"
                          "Orbiting: circling at the Keplerian speed of its radius, with the disk.\n"
                          "Free fall: dropped from rest where the camera is, falls towards the horizon.\n"
                          "A moving camera sees aberration and Doppler shifts of everything.");
    if (settings.observer == Observer::FREE_FALL) {
        ImGui::SliderFloat("Sideways speed", &settings.fallSpeed, -0.95F, 0.95F, "%+.3f c");
        ImGui::SetItemTooltip("Launch speed along the orbit direction, relative to the local ZAMO (+ = with the\n"
                              "hole's rotation). 0 drops it; at the circular speed it orbits; in between it\n"
                              "plunges in a spiral; faster than escape it flies off.");
        if (info.circularSpeed) {
            ImGui::TextDisabled("Circular speed here %+.3f c", *info.circularSpeed);
        }
        actions.dropAgain = ImGui::Button("Drop again");
        ImGui::SameLine();
    }
    ImGui::Text("r %.2f M  v %.3f c", info.observerRadius, info.observerSpeed);
    ImGui::SetItemTooltip("Boyer-Lindquist radius, and speed relative to the local ZAMO.");
    if (info.properTime) {
        ImGui::Text("Proper time %.1f M", *info.properTime);
    }
    if (!info.observerNote.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%.*s", static_cast<int>(info.observerNote.size()), info.observerNote.data());
        ImGui::PopStyleColor();
    }
    ImGui::BeginDisabled(info.lookCentered);
    actions.recenterLook = ImGui::Button("Look at the hole (C)");
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Right-drag looks around; this (or a right double click) turns the view back.");
    ImGui::SliderFloat("Time scale", &settings.timeScale, 0.0F, 50.0F, "%.1f M/s", ImGuiSliderFlags_Logarithmic);
    ImGui::SetItemTooltip("M of time per second for everything that moves: disk turbulence, orbiting and\n"
                          "falling cameras (0 = frozen). For a 10 solar-mass hole, 1 M is 49 microseconds.");
    ImGui::SliderFloat("Vertical FOV", &settings.fovDegrees, 10.0F, 140.0F, "%.0f deg");
}

void ControlPanel::drawBackground(SceneSettings& settings, const PanelInfo& info, PanelActions& actions)
{
    int background {std::to_underlying(settings.background)};
    // Not "Background": that is the section header's label, and ImGui derives IDs from labels.
    if (ImGui::Combo("Sky", &background, backgroundNames.data(), static_cast<int>(backgroundNames.size()))) {
        settings.background = static_cast<Background>(background);
    }

    if (settings.background == Background::IMAGE) {
        const std::string current {settings.skyImage.empty() ? "(choose)" : settings.skyImage.filename().string()};
        if (ImGui::BeginCombo("Map", current.c_str())) {
            for (const std::filesystem::path& image : info.skyImages) {
                const std::string name {image.filename().string()};
                if (ImGui::Selectable(name.c_str(), image == settings.skyImage)) {
                    settings.skyImage = image;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        actions.rescanSkyImages = ImGui::SmallButton("Rescan");
        if (info.skyImages.empty()) {
            ImGui::TextDisabled("No maps in assets/: run scripts/download_sky_maps.sh");
        }
        if (ImGui::InputTextWithHint("Path", "file or cube-map folder, Enter", &m_skyPathInput,
                                     ImGuiInputTextFlags_EnterReturnsTrue)) {
            settings.skyImage = m_skyPathInput;
        }
        ImGui::SetItemTooltip("Equirectangular .hdr/.png/.jpg (2:1), or a folder of six cube faces\n"
                              "(px nx py ny pz nz). Relative paths are looked up in assets/ too.");
        if (!info.skyStatus.empty()) {
            const ImVec4 color {info.skyError ? ImVec4 {1.0F, 0.4F, 0.4F, 1.0F} : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)};
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextWrapped("%.*s", static_cast<int>(info.skyStatus.size()), info.skyStatus.data());
            ImGui::PopStyleColor();
        }
    }

    ImGui::BeginDisabled(settings.background == Background::NONE);
    ImGui::SliderFloat("Sky brightness", &settings.skyBrightnessEv, -6.0F, 6.0F, "%+.1f EV");
    ImGui::SliderFloat("Sky yaw", &settings.skyYawDegrees, 0.0F, 360.0F, "%.0f deg");
    ImGui::SetItemTooltip("Turns the sky about the spin axis.");
    ImGui::SliderFloat("Sky tilt", &settings.skyTiltDegrees, -90.0F, 90.0F, "%.0f deg");
    ImGui::SetItemTooltip("Tilts the sky's pole away from the spin axis.");
    ImGui::EndDisabled();
}

PanelActions ControlPanel::draw(SceneSettings& settings, const PanelInfo& info)
{
    PanelActions actions {};

    ImGui::SetNextWindowPos(ImVec2 {12.0F, 12.0F}, ImGuiCond_FirstUseEver);
    // The width a 13-pixel font needs, scaled with the font (UI scale), whatever imgui.ini remembers.
    const float width {panelWidth * ImGui::GetFontSize() / panelFontSize};
    ImGui::SetNextWindowSize(ImVec2 {width, 0.0F}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2 {width, 0.0F}, ImVec2 {FLT_MAX, FLT_MAX});
    if (!ImGui::Begin("Black hole")) {
        ImGui::End();
        return actions;
    }

    ImGui::TextDisabled("Kerr geodesics, Novikov-Thorne disk");
    if (!info.shaderError.empty()) { // on top, so it cannot scroll out of sight
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4 {1.0F, 0.4F, 0.4F, 1.0F});
        ImGui::TextWrapped("Shader error (previous shaders still active):\n%.*s",
                           static_cast<int>(info.shaderError.size()), info.shaderError.data());
        ImGui::PopStyleColor();
    }

    if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawPresets(info, actions);
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
        ImGui::SliderFloat("Outer radius", &settings.diskOuterRadius, 10.0F, 1000.0F, "%.0f M",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SetItemTooltip("Real disks extend far out and fade as they cool (T ~ r^-3/4); a small radius cuts them.");
        ImGui::SliderFloat("Edge softness", &settings.diskEdgeSoftness, 0.0F, 1.0F, "%.2f r_out");
        ImGui::SetItemTooltip("The outer disk thins out over this fraction of its radius: it turns transparent\n"
                              "(optical depth -> 0) and blends into the sky instead of ending. 0 = sharp edge.");
        ImGui::SliderFloat("Peak temperature", &settings.diskPeakTemperature, 1000.0F, 50000.0F, "%.0f K",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SetItemTooltip("Maximum local temperature of the Novikov-Thorne disk, ~(accretion rate / M^2)^(1/4).");
        ImGui::Checkbox("Doppler shift", &settings.dopplerShift);
        ImGui::SetItemTooltip("Motion of the gas: blueshift and beaming on the approaching side.");
        ImGui::SameLine();
        ImGui::Checkbox("Gravitational", &settings.gravitationalRedshift);
        ImGui::SetItemTooltip("Gravitational shift and frame dragging, of the disk and of the sky.");
        ImGui::Checkbox("Limb darkening", &settings.limbDarkening);
        ImGui::SetItemTooltip("Electron-scattering atmosphere: the gas looks 2.4x dimmer edge-on than face-on\n"
                              "(Chandrasekhar 1960), flux conserved.");
        ImGui::SliderFloat("Turbulence", &settings.diskTurbulence, 0.0F, 1.0F, "sigma %.2f");
        ImGui::SetItemTooltip("MRI-like turbulence, a statistical model: lognormal flux fluctuations (rms of ln F),\n"
                              "mean flux kept at Novikov-Thorne. 0 = the smooth time-averaged disk.");
        ImGui::BeginDisabled(settings.diskTurbulence <= 0.0F);
        ImGui::SliderFloat("Eddy size", &settings.diskEddySize, 0.01F, 0.2F, "%.3f r", ImGuiSliderFlags_Logarithmic);
        ImGui::SetItemTooltip("Size of the turbulent cells, ~ the disk thickness H/r.");
        ImGui::EndDisabled();
        const double a {static_cast<double>(settings.spin)};
        ImGui::Text("Flux peak at r = %.2f M", kerr::novikovThorneFluxPeakRadius(a, settings.diskOrbit));
    }

    if (ImGui::CollapsingHeader("Background", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawBackground(settings, info, actions);
    }

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawCamera(settings, info, actions);
    }

    if (ImGui::CollapsingHeader("Ray tracing", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawRayTracingSettings(settings);
    }

    if (ImGui::CollapsingHeader("Image", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawImageSettings(settings, info);
    }

    if (ImGui::CollapsingHeader("Screenshot", ImGuiTreeNodeFlags_DefaultOpen)) {
        actions.screenshot = ImGui::Button("Save PNG (F12)");
        ImGui::SameLine();
        int size {std::to_underlying(settings.screenshotSize)};
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::Combo("##size", &size, screenshotSizeNames.data(), static_cast<int>(screenshotSizeNames.size()))) {
            settings.screenshotSize = static_cast<ScreenshotSize>(size);
        }
        ImGui::SetItemTooltip("Rendered offscreen at full quality (render scale 1), without the panel,\n"
                              "into screenshots/ in the source tree. 8K takes a few seconds on an iGPU.");
        ImGui::SliderInt("Samples", &settings.screenshotSamples, 1, 64, "%d", ImGuiSliderFlags_Logarithmic);
        ImGui::SetItemTooltip("Jittered frames averaged per screenshot (anti-aliasing); time grows with it.");
        if (!info.screenshotStatus.empty()) {
            const ImVec4 color {info.screenshotError ? ImVec4 {1.0F, 0.4F, 0.4F, 1.0F}
                                                      : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)};
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextWrapped("%.*s", static_cast<int>(info.screenshotStatus.size()), info.screenshotStatus.data());
            ImGui::PopStyleColor();
        }
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
    ImGui::TextDisabled("Drag or wheel click: orbit  Right drag: look  Scroll: zoom  F11: fullscreen  F12: screenshot");

    ImGui::End();
    return actions;
}

} // namespace bh
