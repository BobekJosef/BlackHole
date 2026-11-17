# Third-party dependencies.
#
# GLFW and GLM are taken from the system / vcpkg when a matching package is found
# (FetchContent's FIND_PACKAGE_ARGS), otherwise downloaded and built from source.
# Dear ImGui has no installable CMake package upstream and is always fetched, as is stb_image
# (single header, no releases: pinned to a commit).
# GLAD is generated once and vendored in external/glad.

include(FetchContent)

# --- GLFW: window, OpenGL context, input ------------------------------------
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
# X11 alone runs everywhere on Linux (XWayland on Wayland desktops) and needs fewer
# -dev packages; pass -DGLFW_BUILD_WAYLAND=ON for a native Wayland backend.
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "Build GLFW's native Wayland backend")

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
    SYSTEM
    FIND_PACKAGE_ARGS 3.4 NAMES glfw3)

# --- GLM: vector / matrix math on the C++ side --------------------------------
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
    SYSTEM
    FIND_PACKAGE_ARGS 1.0 NAMES glm)

# --- Dear ImGui: control panel ------------------------------------------------
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
    GIT_SHALLOW TRUE)

# --- stb_image: sky map loading (PNG, JPEG, Radiance .hdr, ...) ----------------
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG fe3a2a73bfd5f38f8117d223896de72cc2aea30c # master of 2026-09-16 (stb_image 2.30)
    GIT_SHALLOW FALSE)

FetchContent_MakeAvailable(glfw glm imgui stb)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui SYSTEM PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imgui_SOURCE_DIR}/misc/cpp)
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
target_link_libraries(imgui PUBLIC glfw PRIVATE ${CMAKE_DL_LIBS})
add_library(imgui::imgui ALIAS imgui)

# --- GLAD: OpenGL function loader (vendored) ----------------------------------
add_subdirectory(${PROJECT_SOURCE_DIR}/external/glad ${PROJECT_BINARY_DIR}/external/glad)

# --- stb_image: the implementation compiled once, in its own target -------------
# The header has no CMake support; the translation unit is generated so that the
# STB_IMAGE_IMPLEMENTATION switch stays a compile definition, not a #define in our sources.
file(CONFIGURE OUTPUT "${PROJECT_BINARY_DIR}/generated/stb_image.c" CONTENT "#include <stb_image.h>\n")
add_library(stb_image STATIC "${PROJECT_BINARY_DIR}/generated/stb_image.c")
target_include_directories(stb_image SYSTEM PUBLIC "${stb_SOURCE_DIR}")
target_compile_definitions(stb_image PRIVATE STB_IMAGE_IMPLEMENTATION STBI_NO_GIF STBI_NO_PSD STBI_NO_PIC)
add_library(stb::image ALIAS stb_image)
