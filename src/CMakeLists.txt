set(BLACKHOLE_SHADER_DIR "${PROJECT_SOURCE_DIR}/shaders")
set(BLACKHOLE_ASSET_DIR "${PROJECT_SOURCE_DIR}/assets")
configure_file(config.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/blackhole/config.hpp" @ONLY)

# GLSL #include preprocessor: no OpenGL, so it is unit-tested on its own.
add_library(bh_shadertools STATIC
    gfx/ShaderPreprocessor.cpp)
target_include_directories(bh_shadertools PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
blackhole_configure_target(bh_shadertools)

# Pure C++ physics (no OpenGL): unit-testable, and the CPU reference for the GPU code.
add_library(bh_physics STATIC
    physics/Blackbody.cpp
    physics/Disk.cpp
    physics/Geodesic.cpp
    physics/Kerr.cpp
    physics/KerrSchild.cpp
    physics/Observer.cpp)
target_include_directories(bh_physics PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
blackhole_configure_target(bh_physics)

# Sky map decoding (stb_image): no OpenGL, so it runs on a worker thread and in tests.
add_library(bh_assets STATIC
    assets/SkyImage.cpp)
target_include_directories(bh_assets PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(bh_assets PRIVATE stb::image)
blackhole_configure_target(bh_assets)

add_executable(blackhole
    main.cpp
    app/Application.cpp
    gfx/GlDebug.cpp
    gfx/GpuTimer.cpp
    gfx/ShaderProgram.cpp
    gfx/Window.cpp
    render/BlackHoleRenderer.cpp
    scene/OrbitCamera.cpp
    ui/ControlPanel.cpp
    ui/ImGuiLayer.cpp
    ui/ShadowOverlay.cpp)

file(GLOB_RECURSE shader_sources CONFIGURE_DEPENDS "${BLACKHOLE_SHADER_DIR}/*")
target_sources(blackhole PRIVATE ${shader_sources})
set_source_files_properties(${shader_sources} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("Shaders" FILES ${shader_sources})

target_include_directories(blackhole PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_definitions(blackhole PRIVATE GLFW_INCLUDE_NONE)
target_link_libraries(blackhole PRIVATE
    bh_assets
    bh_physics
    bh_shadertools
    glad::glad
    glfw
    glm::glm
    imgui::imgui)
blackhole_configure_target(blackhole)
