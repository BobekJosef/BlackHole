add_executable(test_kerr test_kerr.cpp)
target_link_libraries(test_kerr PRIVATE bh_physics)
blackhole_configure_target(test_kerr)
add_test(NAME kerr COMMAND test_kerr)

add_executable(test_geodesic test_geodesic.cpp)
target_link_libraries(test_geodesic PRIVATE bh_physics)
blackhole_configure_target(test_geodesic)
add_test(NAME geodesic COMMAND test_geodesic)

add_executable(test_disk test_disk.cpp)
target_link_libraries(test_disk PRIVATE bh_physics)
blackhole_configure_target(test_disk)
add_test(NAME disk COMMAND test_disk)

add_executable(test_shader_preprocessor test_shader_preprocessor.cpp)
target_link_libraries(test_shader_preprocessor PRIVATE bh_shadertools)
blackhole_configure_target(test_shader_preprocessor)
add_test(NAME shader_preprocessor COMMAND test_shader_preprocessor)

# Offline GLSL validation, when glslangValidator is installed (Debian/Ubuntu: glslang-tools).
# -G (OpenGL SPIR-V target) is the mode in which glslang resolves #include; --aml assigns the
# explicit uniform locations SPIR-V needs. Only the main shaders are compiled; headers via them.
find_program(GLSLANG_VALIDATOR glslangValidator)
if(GLSLANG_VALIDATOR)
    set(shader_dir "${PROJECT_SOURCE_DIR}/shaders")
    file(GLOB shaders CONFIGURE_DEPENDS
        "${shader_dir}/*.vert" "${shader_dir}/*.frag" "${shader_dir}/*.comp")
    foreach(shader IN LISTS shaders)
        cmake_path(GET shader FILENAME name)
        add_test(NAME "glsl:${name}"
            COMMAND "${GLSLANG_VALIDATOR}" -G --aml "-I${shader_dir}" -o /dev/null "${shader}")
    endforeach()
else()
    message(STATUS "glslangValidator not found: shader validation tests disabled")
endif()
