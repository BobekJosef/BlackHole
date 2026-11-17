add_executable(test_kerr test_kerr.cpp)
target_link_libraries(test_kerr PRIVATE bh_physics)
blackhole_configure_target(test_kerr)
add_test(NAME kerr COMMAND test_kerr)
