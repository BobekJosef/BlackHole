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

void ImGuiLayer::beginFrame()
{
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
