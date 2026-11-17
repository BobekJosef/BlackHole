#include "ui/ImGuiLayer.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <cmath>
#include <stdexcept>

namespace bh {

namespace {

constexpr float baseFontPixels = 13.0F; // Dear ImGui's default font size

} // namespace

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
    ImGui::GetStyle() = m_baseStyle;
    ImGui::GetStyle().ScaleAllSizes(m_scale);
    ImGuiIO& io {ImGui::GetIO()};
    io.Fonts->Clear();
    ImFontConfig font {};
    font.SizePixels = std::round(baseFontPixels * m_scale);
    io.Fonts->AddFontDefault(&font);
    // Between frames: the backend uploads the new atlas.
    ImGui_ImplOpenGL3_DestroyFontsTexture();
    ImGui_ImplOpenGL3_CreateFontsTexture();
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

bool ImGuiLayer::wantsMouse() const
{
    return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiLayer::wantsKeyboard() const
{
    return ImGui::GetIO().WantCaptureKeyboard;
}

} // namespace bh
