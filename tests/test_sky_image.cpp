// Image files (bh_assets) on small files written here: sky map formats and pixel values, the
// equirectangular 2:1 rule, cube-map directories with each naming scheme, the error messages, and the PNG
// writer (round trip through the reader, file naming).

#include "assets/PngFile.hpp"
#include "assets/SkyImage.hpp"
#include "Check.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using bh::test::expect;

void writeFile(const std::filesystem::path& path, const std::string_view contents)
{
    std::ofstream file {path, std::ios::binary};
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
}

// Binary PPM (P6), every pixel the same 8-bit colour.
void writePpm(const std::filesystem::path& path, const int width, const int height, const std::array<char, 3> rgb)
{
    std::string contents {"P6\n" + std::to_string(width) + ' ' + std::to_string(height) + "\n255\n"};
    for (int i {0}; i < width * height; ++i) {
        contents.append(rgb.data(), rgb.size());
    }
    writeFile(path, contents);
}

class TemporaryDirectory final {
public:
    TemporaryDirectory() { std::filesystem::create_directories(m_path); }
    ~TemporaryDirectory() { std::filesystem::remove_all(m_path); }
    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    TemporaryDirectory(TemporaryDirectory&&) = delete;
    TemporaryDirectory& operator=(TemporaryDirectory&&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

private:
    std::filesystem::path m_path {std::filesystem::temp_directory_path() / "blackhole_test_sky_image"};
};

void testEquirectangular(const std::filesystem::path& dir)
{
    writePpm(dir / "sky.ppm", 4, 2, {'\xff', '\x80', '\x00'});
    const auto image {bh::loadSkyImage(dir / "sky.ppm")};
    expect(image.has_value(), "8-bit equirectangular map loads");
    if (image) {
        expect(image->projection == bh::SkyProjection::EQUIRECTANGULAR && image->faces.size() == 1, "one face");
        const bh::SkyFace& face {image->faces.front()};
        expect(face.width == 4 && face.height == 2 && !face.isHdr(), "4x2, 8-bit");
        const auto& rgba {std::get<std::vector<std::uint8_t>>(face.pixels)};
        expect(rgba.size() == 32 && rgba[0] == 255 && rgba[1] == 128 && rgba[2] == 0 && rgba[3] == 255,
               "sRGB bytes kept as they are, alpha added");
    }

    writePpm(dir / "square.ppm", 4, 4, {'\x00', '\x00', '\x00'});
    const auto square {bh::loadSkyImage(dir / "square.ppm")};
    expect(!square && square.error().find("2 x height") != std::string::npos, "non-2:1 map is rejected");

    const auto missing {bh::loadSkyImage(dir / "missing.png")};
    expect(!missing && missing.error().find("no such file") != std::string::npos, "missing file is reported");
}

// Radiance RGBE, uncompressed scanlines (allowed for widths below 8): mantissa m with exponent e is
// m 2^(e - 136), so (128, 64, 32, 129) is (1, 0.5, 0.25), up to half a mantissa step (stb_image
// releases after 2.30 decode to the bucket centre, (m + 0.5) 2^(e - 136), like Ward's reader).
void testHdr(const std::filesystem::path& dir)
{
    std::string contents {"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 2\n"};
    contents += std::string {"\x80\x40\x20\x81\x80\x40\x20\x81", 8};
    writeFile(dir / "sky.hdr", contents);
    const auto image {bh::loadSkyImage(dir / "sky.hdr")};
    expect(image.has_value() && image->faces.front().isHdr(), "Radiance .hdr loads as floats");
    if (image && image->faces.front().isHdr()) {
        const auto& rgb {std::get<std::vector<float>>(image->faces.front().pixels)};
        constexpr float halfStep = 0.5F / 128.0F;
        const auto near = [](const float actual, const float expected) {
            return actual >= expected && actual <= expected + halfStep;
        };
        expect(rgb.size() == 6 && near(rgb[0], 1.0F) && near(rgb[1], 0.5F) && near(rgb[2], 0.25F), "RGBE decoded");
    }
}

void testCubeMap(const std::filesystem::path& dir)
{
    for (const std::array<std::string_view, 6>& names :
         {std::array<std::string_view, 6> {"px", "nx", "py", "ny", "pz", "nz"},
          std::array<std::string_view, 6> {"PosX", "NegX", "PosY", "NegY", "PosZ", "NegZ"},
          std::array<std::string_view, 6> {"right", "left", "top", "bottom", "front", "back"}}) {
        const std::filesystem::path cube {dir / ("cube_" + std::string {names[0]})};
        std::filesystem::create_directories(cube);
        for (std::size_t face {0}; face < names.size(); ++face) {
            writePpm(cube / (std::string {names[face]} + ".ppm"), 2, 2, {static_cast<char>(face), '\0', '\0'});
        }
        writeFile(cube / "README.txt", "not a face");
        const auto image {bh::loadSkyImage(cube)};
        expect(image.has_value() && image->projection == bh::SkyProjection::CUBEMAP && image->faces.size() == 6,
               "cube-map directory loads");
        if (image) {
            for (std::size_t face {0}; face < 6; ++face) {
                const auto& rgba {std::get<std::vector<std::uint8_t>>(image->faces[face].pixels)};
                expect(rgba[0] == face, "faces in OpenGL order +x, -x, +y, -y, +z, -z");
            }
        }
    }

    const std::filesystem::path uneven {dir / "cube_uneven"};
    std::filesystem::create_directories(uneven);
    for (const std::string_view name : {"px", "nx", "py", "ny", "pz", "nz"}) {
        writePpm(uneven / (std::string {name} + ".ppm"), name == "nz" ? 4 : 2, 2, {'\0', '\0', '\0'});
    }
    expect(!bh::loadSkyImage(uneven), "faces of different sizes are rejected");

    const std::filesystem::path incomplete {dir / "not_a_cube"};
    std::filesystem::create_directories(incomplete);
    writePpm(incomplete / "px.ppm", 2, 2, {'\0', '\0', '\0'});
    expect(!bh::cubeMapFaces(incomplete), "five faces missing: not a cube map");
    expect(!bh::loadSkyImage(incomplete), "and it does not load");
}

void testFind(const std::filesystem::path& dir)
{
    const std::vector<std::filesystem::path> found {bh::findSkyImages(dir)};
    const auto has = [&](const std::string_view name) { return std::ranges::find(found, dir / name) != found.end(); };
    expect(has("sky.ppm") && has("sky.hdr") && has("cube_px") && has("cube_right"), "maps and cube maps listed");
    expect(!has("not_a_cube"), "other directories are not");
    expect(std::ranges::is_sorted(found), "sorted");
}

void testPngWriter(const std::filesystem::path& dir)
{
    // 4x2 so the reader takes it as an equirectangular map; every pixel distinct, rows top first.
    bh::RgbaImage image {.width = 4, .height = 2, .pixels = {}};
    for (int i {0}; i < 8; ++i) {
        image.pixels.insert(image.pixels.end(),
                            {static_cast<std::uint8_t>(30 * i), static_cast<std::uint8_t>(7 * i),
                             static_cast<std::uint8_t>(255 - 30 * i), 255});
    }
    const std::filesystem::path path {dir / "shots" / "round_trip.png"};
    expect(bh::writePng(path, image).has_value(), "PNG written (missing directory created)");
    const auto read {bh::loadSkyImage(path)};
    expect(read.has_value() && !read->faces.front().isHdr(), "and read back");
    if (read && !read->faces.front().isHdr()) {
        expect(std::get<std::vector<std::uint8_t>>(read->faces.front().pixels) == image.pixels,
               "pixels survive the round trip, top row first");
    }
    expect(!bh::writePng(dir / "bad.png", bh::RgbaImage {.width = 3, .height = 3, .pixels = {1, 2, 3}}),
           "a pixel count that does not match the size is rejected");

    const std::filesystem::path first {bh::timestampedPath(dir / "shots", "blackhole")};
    expect(first.extension() == ".png" && first.filename().string().starts_with("blackhole_"), "timestamped name");
    expect(bh::writePng(first, image).has_value(), "written");
    const std::filesystem::path second {bh::timestampedPath(dir / "shots", "blackhole")};
    expect(second != first, "never overwrites: a suffix when the name is taken");
}

} // namespace

TEST_CASE("sky maps and PNG files")
{
    // One directory for all: the listing checks see the files the others wrote.
    const TemporaryDirectory dir {};
    testEquirectangular(dir.path());
    testHdr(dir.path());
    testCubeMap(dir.path());
    testFind(dir.path());
    testPngWriter(dir.path());
}
