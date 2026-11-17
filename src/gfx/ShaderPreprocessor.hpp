#pragma once
// Minimal GLSL preprocessor: resolves #include "file" (relative to the including file),
// which OpenGL itself does not support. Each file becomes its own GLSL source-string number
// via #line, so driver logs can be mapped back to file names with annotateShaderLog.
//
// Shader files declare `#extension GL_GOOGLE_include_directive : enable` so that
// glslangValidator accepts them too; that line is blanked here, since drivers do not know it.
// Included files use classic #ifndef guards; this preprocessor does not deduplicate.

#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bh::gl {

struct PreprocessedShader {
    std::string source {};
    // files[i] is GLSL source-string number i; files[0] is the main file.
    std::vector<std::filesystem::path> files {};
};

[[nodiscard]] std::expected<PreprocessedShader, std::string> preprocessShader(const std::filesystem::path& mainFile);

// Replaces source-string numbers in a compiler log ("0:12(5): error", "1(12) : error",
// "ERROR: 2:12:") by the file names.
[[nodiscard]] std::string annotateShaderLog(std::string_view log, std::span<const std::filesystem::path> files);

} // namespace bh::gl
