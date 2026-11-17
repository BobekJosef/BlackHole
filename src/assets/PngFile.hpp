#pragma once
// Writing PNG files (screenshots). No OpenGL, so encoding, which takes a while for large images, can run
// on a worker thread.

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace bh {

// 8-bit RGBA pixels, top row first.
struct RgbaImage {
    int width {};
    int height {};
    std::vector<std::uint8_t> pixels {}; // 4 * width * height
};

// Writes `image` as an 8-bit RGBA PNG, creating missing parent directories.
[[nodiscard]] std::expected<void, std::string> writePng(const std::filesystem::path& path, const RgbaImage& image);

// `stem` + "_" + local time (YYYYMMDD-HHMMSS) + ".png" in `directory`, with "-2", "-3", ... appended while
// such a file already exists.
[[nodiscard]] std::filesystem::path timestampedPath(const std::filesystem::path& directory, std::string_view stem);

} // namespace bh
