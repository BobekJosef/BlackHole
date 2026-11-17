#pragma once

namespace bh::gl {

// Routes KHR_debug messages (core since OpenGL 4.3) to std::cerr. Call with a debug
// context current; notifications are filtered out.
void enableDebugOutput();

} // namespace bh::gl
