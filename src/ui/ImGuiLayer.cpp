#include "ui/ImGuiLayer.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <stdexcept>

namespace bh {

ImGuiLayer::ImGuiLayer(GLFWwindow* const window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    m_baseStyle = ImGui::GetStyle();
    // install_callbacks = true: the backend chains to callbacks installed before it.
    if (!ImGui_ImplGlfw_InitForOpenGL(window, true) || !ImGui_ImplOpenGL3_Init("#version 430 core")) {
        throw std::runtime_error {"Dear ImGui backend initialisation failed"};
    }
}

ImGuiLayer::~ImGuiLayer()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiLayer::setScale(const float scale)
{
    m_scale = scale;
}

void ImGuiLayer::applyScale()
{
    // Sizes from the unscaled style, so rescaling never compounds. Fonts are rasterised at whatever size
    // is drawn (Dear ImGui 1.92+), so the font scale alone keeps text sharp.
    ImGuiStyle& style {ImGui::GetStyle()};
    style = m_baseStyle;
    style.ScaleAllSizes(m_scale);
    style.FontScaleDpi = m_scale;
    m_appliedScale = m_scale;
}

void ImGuiLayer::beginFrame()
{
    if (m_scale != m_appliedScale) {
        applyScale();
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::endFrame()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiLayer::setMouseEnabled(const bool enabled)
{
    ImGuiIO& io {ImGui::GetIO()};
    if (enabled) {
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    } else {
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    }
}

bool ImGuiLayer::wantsMouse() const
{
    return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiLayer::wantsKeyboard() const
{
    return ImGui::GetIO().WantCaptureKeyboard;
}

} // namespace bh
