# doctest: each TEST_CASE becomes its own CTest test ("<suite>/<case>"). Its main() is compiled once,
# from a generated translation unit, so DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN stays a compile definition.
include(FetchContent)
set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)
FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.5.3
    GIT_SHALLOW TRUE
    SYSTEM)
FetchContent_MakeAvailable(doctest)
include("${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")

file(CONFIGURE OUTPUT "${PROJECT_BINARY_DIR}/generated/doctest_main.cpp" CONTENT "#include <doctest/doctest.h>\n")
add_library(bh_test_main OBJECT "${PROJECT_BINARY_DIR}/generated/doctest_main.cpp")
target_link_libraries(bh_test_main PUBLIC doctest::doctest)
target_compile_definitions(bh_test_main PRIVATE DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN)

# blackhole_add_test(<suite> <libraries...>): test_<suite>.cpp, linked with the libraries under test.
function(blackhole_add_test suite)
    add_executable(test_${suite} test_${suite}.cpp)
    target_link_libraries(test_${suite} PRIVATE bh_test_main doctest::doctest ${ARGN})
    target_include_directories(test_${suite} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    blackhole_configure_target(test_${suite})
    doctest_discover_tests(test_${suite} TEST_PREFIX "${suite}/")
endfunction()

blackhole_add_test(kerr bh_physics)
blackhole_add_test(geodesic bh_physics)
blackhole_add_test(observer bh_physics)
blackhole_add_test(disk bh_physics)
blackhole_add_test(sky_image bh_assets)
blackhole_add_test(preset bh_presets)
target_compile_definitions(test_preset PRIVATE BLACKHOLE_PRESET_DIR="${PROJECT_SOURCE_DIR}/presets")
blackhole_add_test(shader_preprocessor bh_shadertools)

# The GPU tracer against the CPU reference (needs an OpenGL 4.3 GPU and a display, so off by default and in
# CI): cmake --preset debug -DBLACKHOLE_GPU_TESTS=ON.
if(BLACKHOLE_GPU_TESTS)
    add_test(NAME gpu_reference COMMAND blackhole --validate)
    set_tests_properties(gpu_reference PROPERTIES LABELS gpu)
endif()

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
