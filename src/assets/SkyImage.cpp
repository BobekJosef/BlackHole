#include "assets/SkyImage.hpp"

#include <stb_image.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <format>
#include <memory>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>

namespace bh {

namespace {

constexpr std::array<std::string_view, 7> imageExtensions {".hdr", ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".ppm"};

// Face names in OpenGL's order +x, -x, +y, -y, +z, -z, one row per naming scheme in common use.
constexpr std::array<std::array<std::string_view, 6>, 3> cubeFaceNames {{
    {"px", "nx", "py", "ny", "pz", "nz"},
    {"posx", "negx", "posy", "negy", "posz", "negz"},
    {"right", "left", "top", "bottom", "front", "back"},
}};

struct StbiDeleter {
    void operator()(void* const pixels) const noexcept { stbi_image_free(pixels); }
};

[[nodiscard]] std::string lowercase(std::string text)
{
    std::ranges::transform(text, text.begin(),
                           [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

[[nodiscard]] bool isImageFile(const std::filesystem::path& path)
{
    std::error_code error {};
    return std::filesystem::is_regular_file(path, error)
        && std::ranges::find(imageExtensions, lowercase(path.extension().string())) != imageExtensions.end();
}

// sRGB electro-optical transfer function (IEC 61966-2-1): encoded value in [0, 1] -> linear light.
[[nodiscard]] float srgbToLinear(const float encoded)
{
    return encoded <= 0.04045F ? encoded / 12.92F : std::pow((encoded + 0.055F) / 1.055F, 2.4F);
}

[[nodiscard]] std::string failure(const std::filesystem::path& path)
{
    return std::format("{}: {}", path.filename().string(), stbi_failure_reason());
}

[[nodiscard]] std::expected<SkyFace, std::string> loadFace(const std::filesystem::path& path)
{
    const std::string name {path.string()};
    int width {};
    int height {};
    int channels {};
    const auto size = [&] { return static_cast<std::size_t>(width) * static_cast<std::size_t>(height); };

    if (stbi_is_hdr(name.c_str()) != 0) {
        const std::unique_ptr<float, StbiDeleter> data {stbi_loadf(name.c_str(), &width, &height, &channels, 3)};
        if (!data) {
            return std::unexpected {failure(path)};
        }
        const std::span<const float> rgb {data.get(), 3 * size()};
        return SkyFace {.width = width, .height = height, .pixels = std::vector<float>(rgb.begin(), rgb.end())};
    }
    if (stbi_is_16_bit(name.c_str()) != 0) {
        // No 16-bit sRGB texture format exists: decode to linear floats here.
        const std::unique_ptr<stbi_us, StbiDeleter> data {stbi_load_16(name.c_str(), &width, &height, &channels, 3)};
        if (!data) {
            return std::unexpected {failure(path)};
        }
        const std::span<const stbi_us> encoded {data.get(), 3 * size()};
        std::vector<float> rgb(encoded.size());
        std::ranges::transform(encoded, rgb.begin(),
                               [](const stbi_us value) { return srgbToLinear(static_cast<float>(value) / 65535.0F); });
        return SkyFace {.width = width, .height = height, .pixels = std::move(rgb)};
    }
    const std::unique_ptr<stbi_uc, StbiDeleter> data {stbi_load(name.c_str(), &width, &height, &channels, 4)};
    if (!data) {
        return std::unexpected {failure(path)};
    }
    const std::span<const stbi_uc> rgba {data.get(), 4 * size()};
    return SkyFace {.width = width, .height = height, .pixels = std::vector<std::uint8_t>(rgba.begin(), rgba.end())};
}

[[nodiscard]] std::expected<SkyImage, std::string> loadEquirectangular(const std::filesystem::path& path)
{
    auto face {loadFace(path)};
    if (!face) {
        return std::unexpected {std::move(face.error())};
    }
    if (face->width != 2 * face->height) {
        return std::unexpected {std::format("{}: {}x{} is not an equirectangular map (width must be 2 x height)",
                                            path.filename().string(), face->width, face->height)};
    }
    SkyImage image {.projection = SkyProjection::EQUIRECTANGULAR, .faces = {}};
    image.faces.push_back(std::move(*face));
    return image;
}

[[nodiscard]] std::expected<SkyImage, std::string> loadCubeMap(const std::filesystem::path& directory,
                                                               const std::array<std::filesystem::path, 6>& files)
{
    SkyImage image {.projection = SkyProjection::CUBEMAP, .faces = {}};
    for (const std::filesystem::path& file : files) {
        auto face {loadFace(file)};
        if (!face) {
            return std::unexpected {std::move(face.error())};
        }
        const SkyFace& first {image.faces.empty() ? *face : image.faces.front()};
        if (face->width != face->height || face->width != first.width || face->isHdr() != first.isHdr()) {
            return std::unexpected {std::format("{}: cube faces must be square, of one size and one format",
                                                directory.filename().string())};
        }
        image.faces.push_back(std::move(*face));
    }
    return image;
}

} // namespace

std::optional<std::array<std::filesystem::path, 6>> cubeMapFaces(const std::filesystem::path& directory)
{
    std::error_code error {};
    if (!std::filesystem::is_directory(directory, error)) {
        return std::nullopt;
    }
    std::vector<std::filesystem::path> images {};
    for (const auto& entry : std::filesystem::directory_iterator {directory, error}) {
        if (isImageFile(entry.path())) {
            images.push_back(entry.path());
        }
    }
    const auto stem = [](const std::filesystem::path& file) { return lowercase(file.stem().string()); };
    for (const auto& names : cubeFaceNames) {
        std::array<std::filesystem::path, 6> faces {};
        std::size_t matched {0};
        for (; matched < faces.size(); ++matched) {
            const auto it {std::ranges::find(images, names[matched], stem)};
            if (it == images.end()) {
                break;
            }
            faces[matched] = *it;
        }
        if (matched == faces.size()) {
            return faces;
        }
    }
    return std::nullopt;
}

std::expected<SkyImage, std::string> loadSkyImage(const std::filesystem::path& path)
{
    if (const auto faces {cubeMapFaces(path)}) {
        return loadCubeMap(path, *faces);
    }
    std::error_code error {};
    if (std::filesystem::is_directory(path, error)) {
        return std::unexpected {std::format("{}: no cube faces (px/nx/py/ny/pz/nz, posx/negx/..., or "
                                            "right/left/top/bottom/front/back) in this directory",
                                            path.filename().string())};
    }
    if (!std::filesystem::is_regular_file(path, error)) {
        return std::unexpected {std::format("{}: no such file", path.string())};
    }
    return loadEquirectangular(path);
}

std::vector<std::filesystem::path> findSkyImages(const std::filesystem::path& directory)
{
    std::vector<std::filesystem::path> found {};
    std::error_code error {};
    for (const auto& entry : std::filesystem::directory_iterator {directory, error}) {
        if (isImageFile(entry.path()) || cubeMapFaces(entry.path())) {
            found.push_back(entry.path());
        }
    }
    std::ranges::sort(found);
    return found;
}

} // namespace bh
