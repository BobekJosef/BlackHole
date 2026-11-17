// Presets (scene/Preset.hpp): the text format round-trips every preset field, keys a file leaves out take
// their defaults, settings outside presets (VSync, render scale, debug views) survive a load, malformed
// lines and out-of-range values are errors naming the line, unknown keys are warnings, and every built-in
// preset in the directory given as argv[1] loads cleanly.

#include "scene/Preset.hpp"

#include <filesystem>
#include <iostream>
#include <source_location>
#include <string>
#include <string_view>

namespace {

int g_failures {0};

void expect(const bool condition, const std::string_view what,
            const std::source_location where = std::source_location::current())
{
    if (!condition) {
        ++g_failures;
        std::cerr << where.file_name() << ':' << where.line() << ": " << what << '\n';
    }
}

[[nodiscard]] bh::Preset unusual()
{
    bh::Preset preset {};
    auto& s {preset.settings};
    s.spin = 0.75F;
    s.diskEnabled = false;
    s.diskOrbit = bh::kerr::Orbit::RETROGRADE;
    s.diskOuterRadius = 123.5F;
    s.diskEdgeSoftness = 0.25F;
    s.diskPeakTemperature = 12345.0F;
    s.dopplerShift = false;
    s.gravitationalRedshift = false;
    s.limbDarkening = false;
    s.diskTurbulence = 0.8F;
    s.diskEddySize = 0.1F;
    s.background = bh::Background::IMAGE;
    s.skyImage = "maps/sky with spaces.hdr";
    s.skyBrightnessEv = -1.5F;
    s.skyYawDegrees = 42.0F;
    s.skyTiltDegrees = -12.0F;
    s.observer = bh::Observer::FREE_FALL;
    s.fallSpeed = -0.3F;
    s.fovDegrees = 75.0F;
    s.timeScale = 0.1F;
    s.toneMapper = bh::ToneMapper::ACES;
    s.exposureEv = 1.25F;
    s.autoExposure = true;
    s.bloomEnabled = false;
    s.bloomStrength = 0.1F;
    preset.camera = bh::CameraPlacement {.azimuthDegrees = -33.3F, .polarDegrees = 12.5F, .distance = 7.0F,
                                         .lookYawDegrees = 20.0F, .lookPitchDegrees = -5.5F};
    return preset;
}

void testRoundTrip()
{
    const bh::Preset original {unusual()};
    const std::string text {bh::formatPreset(original)};
    const auto parsed {bh::parsePreset(text, bh::Preset {})};
    expect(parsed.has_value() && parsed->warnings.empty(), "formatted preset parses cleanly");
    if (parsed) {
        expect(bh::formatPreset(parsed->preset) == text, "every field round-trips exactly");
        expect(parsed->preset.camera == original.camera, "camera placement");
        expect(parsed->preset.settings.skyImage == original.settings.skyImage, "path with spaces");
    }
}

void testDefaultsAndKeptSettings()
{
    bh::Preset current {unusual()};
    current.settings.vsync = false;
    current.settings.renderScale = 0.5F;
    current.settings.debugView = bh::DebugView::STEPS;
    current.settings.maxSteps = 1234;
    const auto parsed {bh::parsePreset("spin = 0.5\n", current)};
    expect(parsed.has_value(), "one-line preset");
    if (parsed) {
        const bh::SceneSettings defaults {};
        const auto& s {parsed->preset.settings};
        expect(s.spin == 0.5F, "the given key");
        expect(s.diskOuterRadius == defaults.diskOuterRadius && s.observer == defaults.observer
                   && parsed->preset.camera == bh::CameraPlacement {},
               "keys left out take their defaults");
        expect(!s.vsync && s.renderScale == 0.5F && s.debugView == bh::DebugView::STEPS && s.maxSteps == 1234,
               "settings outside presets are kept");
    }
}

void testSyntax()
{
    const auto parsed {bh::parsePreset("# comment\r\n\r\n   spin   =  0.25  \r\n# spin = 0.9\nnot.a.key = 3\n",
                                       bh::Preset {})};
    expect(parsed.has_value() && parsed->preset.settings.spin == 0.25F, "comments, blank lines, spaces, CRLF");
    expect(parsed.has_value() && parsed->warnings.size() == 1
               && parsed->warnings.front().find("line 5") != std::string::npos,
           "unknown key: a warning with its line");

    const auto failsWith = [](const std::string_view text, const std::string_view part) {
        const auto result {bh::parsePreset(text, bh::Preset {})};
        return !result && result.error().find(part) != std::string::npos;
    };
    expect(failsWith("spin = 0.5\nspin = 2\n", "line 2: spin = 2 is outside [0, 0.998]"), "out of range");
    expect(failsWith("disk.turbulence = lots\n", "must be a number"), "not a number");
    expect(failsWith("disk.turbulence = 0.5x\n", "must be a number"), "trailing junk");
    expect(failsWith("disk.orbit = sideways\n", "one of: prograde, retrograde"), "unknown enum value");
    expect(failsWith("image.bloom = yes\n", "true or false"), "not a bool");
    expect(failsWith("just words\n", "line 1: expected 'key = value'"), "no '='");
}

void testFiles(const std::filesystem::path& builtIn)
{
    const std::filesystem::path dir {std::filesystem::temp_directory_path() / "blackhole_test_preset"};
    std::filesystem::remove_all(dir);
    const bh::Preset original {unusual()};
    expect(bh::savePreset(dir / "mine.preset", original).has_value(), "saved (directory created)");
    const auto loaded {bh::loadPreset(dir / "mine.preset", bh::Preset {})};
    expect(loaded.has_value() && bh::formatPreset(loaded->preset) == bh::formatPreset(original), "loaded back");
    expect(!bh::loadPreset(dir / "missing.preset", bh::Preset {}), "missing file is an error");
    const auto found {bh::findPresets(dir)};
    expect(found.size() == 1 && found.front().filename() == "mine.preset", "found by extension");
    std::filesystem::remove_all(dir);

    expect(bh::isValidPresetName("Edge-on view_2"), "plain name");
    expect(!bh::isValidPresetName("") && !bh::isValidPresetName("../x") && !bh::isValidPresetName("a/b")
               && !bh::isValidPresetName(" padded") && !bh::isValidPresetName("dot.ted"),
           "no paths, extensions or padding");

    const auto presets {bh::findPresets(builtIn)};
    expect(!presets.empty(), "built-in presets exist");
    for (const std::filesystem::path& path : presets) {
        const auto preset {bh::loadPreset(path, bh::Preset {})};
        expect(preset.has_value(), std::string {"loads: "} + path.filename().string()
                                       + (preset ? std::string {} : ": " + preset.error()));
        expect(preset.has_value() && preset->warnings.empty(), "no warnings: " + path.filename().string());
    }
}

} // namespace

int main(const int argc, const char* const* const argv)
{
    if (argc != 2) {
        std::cerr << "usage: test_preset <built-in preset directory>\n";
        return 2;
    }
    testRoundTrip();
    testDefaultsAndKeptSettings();
    testSyntax();
    // argv is the C entry point's array: indexing it is the only way in.
    testFiles(std::filesystem::path {argv[1]});

    if (g_failures != 0) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all preset checks passed\n";
    return 0;
}
