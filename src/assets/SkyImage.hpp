#pragma once
// Sky maps for the background, decoded on the CPU. No OpenGL, so a map can be decoded on a worker
// thread while the app keeps rendering, and the loader is unit-tested without a GL context.

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace bh {

enum class SkyProjection { EQUIRECTANGULAR, CUBEMAP };

// One decoded image, top row first (which is also OpenGL's t = 0 for the way the shaders sample it).
struct SkyFace {
    int width {};
    int height {};
    // Linear-light RGB floats (Radiance .hdr; 16-bit PNGs decoded from sRGB), or 8-bit sRGB-encoded RGBA,
    // which the GPU decodes when sampling (GL_SRGB8_ALPHA8).
    std::variant<std::vector<float>, std::vector<std::uint8_t>> pixels {};

    [[nodiscard]] bool isHdr() const { return std::holds_alternative<std::vector<float>>(pixels); }
};

struct SkyImage {
    SkyProjection projection {SkyProjection::EQUIRECTANGULAR};
    // One equirectangular image, or six cube faces in OpenGL's order +x, -x, +y, -y, +z, -z.
    std::vector<SkyFace> faces {};
};

// The six face files of a cube map stored as a directory, in OpenGL's order, or nothing if `directory`
// is not one. Face files are named (case-insensitive, any supported extension) px/nx/py/ny/pz/nz,
// posx/negx/posy/negy/posz/negz, or right/left/top/bottom/front/back (+x, -x, +y, -y, +z, -z).
[[nodiscard]] std::optional<std::array<std::filesystem::path, 6>> cubeMapFaces(const std::filesystem::path& directory);

// A file is an equirectangular map (width = 2 x height); a directory is a cube map (cubeMapFaces,
// square faces of equal size). Formats: Radiance .hdr (HDR), PNG (8 or 16 bit), JPEG, TGA, BMP, PNM.
[[nodiscard]] std::expected<SkyImage, std::string> loadSkyImage(const std::filesystem::path& path);

// The sky maps directly in `directory`: image files and cube-map directories, sorted by name.
[[nodiscard]] std::vector<std::filesystem::path> findSkyImages(const std::filesystem::path& directory);

} // namespace bh
