#include "assets/PngFile.hpp"

#include <stb_image_write.h>

#include <chrono>
#include <format>
#include <stdexcept>
#include <system_error>

namespace bh {

std::expected<void, std::string> writePng(const std::filesystem::path& path, const RgbaImage& image)
{
    constexpr int channels = 4;
    if (image.width <= 0 || image.height <= 0
        || image.pixels.size()
               != static_cast<std::size_t>(channels) * static_cast<std::size_t>(image.width)
                      * static_cast<std::size_t>(image.height)) {
        return std::unexpected {std::format("{}: invalid {}x{} image", path.filename().string(), image.width,
                                            image.height)};
    }
    std::error_code error {};
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            return std::unexpected {std::format("{}: {}", path.parent_path().string(), error.message())};
        }
    }
    const std::string name {path.string()};
    if (stbi_write_png(name.c_str(), image.width, image.height, channels, image.pixels.data(), channels * image.width)
        == 0) {
        return std::unexpected {std::format("{}: could not be written", name)};
    }
    return {};
}

std::filesystem::path timestampedPath(const std::filesystem::path& directory, const std::string_view stem)
{
    const auto now {std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};
    const std::string stamp {[&] {
        try {
            return std::format("{:%Y%m%d-%H%M%S}", std::chrono::zoned_time {std::chrono::current_zone(), now});
        } catch (const std::runtime_error&) { // no time zone database: UTC
            return std::format("{:%Y%m%d-%H%M%S}Z", now);
        }
    }()};
    std::filesystem::path path {directory / std::format("{}_{}.png", stem, stamp)};
    std::error_code error {};
    for (int suffix {2}; std::filesystem::exists(path, error); ++suffix) {
        path = directory / std::format("{}_{}-{}.png", stem, stamp, suffix);
    }
    return path;
}

} // namespace bh
