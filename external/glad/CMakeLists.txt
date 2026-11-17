# GLAD 2 OpenGL loader, generated once and vendored (see README.md in this directory).
add_library(glad STATIC src/gl.c)
target_include_directories(glad SYSTEM PUBLIC include)
add_library(glad::glad ALIAS glad)
