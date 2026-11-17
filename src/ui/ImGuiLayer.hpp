#pragma once

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

    void beginFrame();
    void endFrame();

    // True while ImGui uses the input, so the camera must ignore it.
    [[nodiscard]] bool wantsMouse() const;
    [[nodiscard]] bool wantsKeyboard() const;
};

} // namespace bh
