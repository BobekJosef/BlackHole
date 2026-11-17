#pragma once

#include <imgui.h>

struct GLFWwindow;

namespace bh {

// Owns the Dear ImGui context and its GLFW + OpenGL 3 backends.
class ImGuiLayer final {
public:
    explicit ImGuiLayer(GLFWwindow* window);
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;
    ImGuiLayer(ImGuiLayer&&) = delete;
    ImGuiLayer& operator=(ImGuiLayer&&) = delete;

    // Scales the whole UI (sizes and font, which stays sharp: it is rasterised at the size drawn), e.g. by
    // the monitor's content scale. Takes effect at the next beginFrame(); cheap when unchanged.
    void setScale(float scale);

    void beginFrame();
    void endFrame();

    // Off while the cursor is captured (hidden, unbounded): its virtual position must not hover or click
    // the panel. Dear ImGui's GLFW backend leaves that to the application.
    void setMouseEnabled(bool enabled);

    // True while ImGui uses the input, so the camera must ignore it.
    [[nodiscard]] bool wantsMouse() const;
    [[nodiscard]] bool wantsKeyboard() const;

private:
    void applyScale();

    ImGuiStyle m_baseStyle {}; // unscaled, so rescaling never compounds
    float m_scale {1.0F};
    float m_appliedScale {0.0F}; // 0: nothing applied yet
};

} // namespace bh
