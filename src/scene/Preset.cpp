#include "scene/Preset.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>
#include <system_error>
#include <type_traits>

namespace bh {

namespace {

// Allowed range of a number: the panel's slider limits.
struct Range {
    float min {};
    float max {};
};
// For fields that are not numbers.
struct NoRange {};

[[nodiscard]] bool inRange(const float value, const Range range)
{
    return value >= range.min && value <= range.max;
}

template <typename T>
[[nodiscard]] bool inRange(const T& /*value*/, NoRange /*range*/)
{
    return true;
}

[[nodiscard]] std::string describe(const Range range)
{
    return std::format("[{}, {}]", range.min, range.max);
}

[[nodiscard]] std::string describe(NoRange /*range*/)
{
    return {};
}

template <typename E>
struct EnumName {
    E value {};
    std::string_view name {};
};

[[nodiscard]] constexpr auto enumNames(kerr::Orbit)
{
    return std::array {EnumName<kerr::Orbit> {kerr::Orbit::PROGRADE, "prograde"},
                       EnumName<kerr::Orbit> {kerr::Orbit::RETROGRADE, "retrograde"}};
}

[[nodiscard]] constexpr auto enumNames(Background)
{
    return std::array {
        EnumName<Background> {Background::NONE, "none"}, EnumName<Background> {Background::STARS, "stars"},
        EnumName<Background> {Background::GRID, "grid"}, EnumName<Background> {Background::IMAGE, "image"}};
}

[[nodiscard]] constexpr auto enumNames(Observer)
{
    return std::array {EnumName<Observer> {Observer::STATIC, "static"},
                       EnumName<Observer> {Observer::ORBITING, "orbiting"},
                       EnumName<Observer> {Observer::FREE_FALL, "free_fall"}};
}

[[nodiscard]] constexpr auto enumNames(ToneMapper)
{
    return std::array {EnumName<ToneMapper> {ToneMapper::AGX, "agx"}, EnumName<ToneMapper> {ToneMapper::ACES, "aces"},
                       EnumName<ToneMapper> {ToneMapper::EXPONENTIAL, "exponential"},
                       EnumName<ToneMapper> {ToneMapper::CLAMP, "clamp"}};
}

// The one list of preset fields, in file order; used for writing, reading and resetting to defaults.
// `Self` is Preset or const Preset; visit(key, member, range), range NoRange for non-numbers.
template <typename Self, typename Visitor>
void visitFields(Self& preset, Visitor&& visit)
{
    auto& s {preset.settings};
    auto& c {preset.camera};
    visit("spin", s.spin, Range {0.0F, 0.998F});
    visit("disk.enabled", s.diskEnabled, NoRange {});
    visit("disk.orbit", s.diskOrbit, NoRange {});
    visit("disk.outer_radius", s.diskOuterRadius, Range {10.0F, 1000.0F});
    visit("disk.edge_softness", s.diskEdgeSoftness, Range {0.0F, 1.0F});
    visit("disk.peak_temperature", s.diskPeakTemperature, Range {1000.0F, 50000.0F});
    visit("disk.doppler", s.dopplerShift, NoRange {});
    visit("disk.gravitational", s.gravitationalRedshift, NoRange {});
    visit("disk.limb_darkening", s.limbDarkening, NoRange {});
    visit("disk.turbulence", s.diskTurbulence, Range {0.0F, 1.0F});
    visit("disk.eddy_size", s.diskEddySize, Range {0.01F, 0.2F});
    visit("sky.background", s.background, NoRange {});
    visit("sky.image", s.skyImage, NoRange {});
    visit("sky.brightness_ev", s.skyBrightnessEv, Range {-6.0F, 6.0F});
    visit("sky.yaw", s.skyYawDegrees, Range {0.0F, 360.0F});
    visit("sky.tilt", s.skyTiltDegrees, Range {-90.0F, 90.0F});
    visit("camera.observer", s.observer, NoRange {});
    visit("camera.fall_speed", s.fallSpeed, Range {-0.95F, 0.95F});
    visit("camera.azimuth", c.azimuthDegrees, Range {-360.0F, 360.0F});
    visit("camera.polar", c.polarDegrees, Range {0.0F, 180.0F});
    visit("camera.distance", c.distance, Range {3.0F, 1000.0F});
    visit("camera.look_yaw", c.lookYawDegrees, Range {-180.0F, 180.0F});
    visit("camera.look_pitch", c.lookPitchDegrees, Range {-89.0F, 89.0F});
    visit("camera.fov", s.fovDegrees, Range {10.0F, 140.0F});
    visit("time_scale", s.timeScale, Range {0.0F, 50.0F});
    visit("image.tone_mapper", s.toneMapper, NoRange {});
    visit("image.auto_exposure", s.autoExposure, NoRange {});
    visit("image.exposure_ev", s.exposureEv, Range {-6.0F, 6.0F});
    visit("image.bloom", s.bloomEnabled, NoRange {});
    visit("image.bloom_strength", s.bloomStrength, Range {0.0F, 0.3F});
}

[[nodiscard]] std::string formatValue(const float value)
{
    return std::format("{}", value); // shortest form that reads back exactly
}

[[nodiscard]] std::string formatValue(const bool value)
{
    return value ? "true" : "false";
}

[[nodiscard]] std::string formatValue(const std::filesystem::path& value)
{
    return value.generic_string();
}

template <typename E>
    requires std::is_enum_v<E>
[[nodiscard]] std::string formatValue(const E value)
{
    for (const auto& [enumValue, name] : enumNames(E {})) {
        if (enumValue == value) {
            return std::string {name};
        }
    }
    return {};
}

// Parsing: nothing on failure; the caller words the error.
[[nodiscard]] std::optional<float> parseValue(const std::string_view text, float /*tag*/)
{
    float value {};
    const auto [end, error] {std::from_chars(text.data(), text.data() + text.size(), value)};
    if (error != std::errc {} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::optional<bool> parseValue(const std::string_view text, bool /*tag*/)
{
    if (text == "true") {
        return true;
    }
    if (text == "false") {
        return false;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::filesystem::path> parseValue(const std::string_view text,
                                                              const std::filesystem::path& /*tag*/)
{
    return std::filesystem::path {text};
}

template <typename E>
    requires std::is_enum_v<E>
[[nodiscard]] std::optional<E> parseValue(const std::string_view text, E /*tag*/)
{
    for (const auto& [value, name] : enumNames(E {})) {
        if (name == text) {
            return value;
        }
    }
    return std::nullopt;
}

template <typename T>
[[nodiscard]] std::string expectedForm(const T& /*tag*/)
{
    if constexpr (std::is_same_v<T, float>) {
        return "a number";
    } else if constexpr (std::is_same_v<T, bool>) {
        return "true or false";
    } else if constexpr (std::is_enum_v<T>) {
        std::string names {};
        for (const auto& [value, name] : enumNames(T {})) {
            names += names.empty() ? "" : ", ";
            names += name;
        }
        return "one of: " + names;
    } else {
        return "a path";
    }
}

[[nodiscard]] std::string_view trim(std::string_view text)
{
    constexpr std::string_view whitespace {" \t\r"};
    const auto first {text.find_first_not_of(whitespace)};
    if (first == std::string_view::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(whitespace) - first + 1);
}

} // namespace

std::string formatPreset(const Preset& preset)
{
    std::string text {"# BlackHole preset: key = value; keys left out take their default values.\n"};
    visitFields(preset, [&text](const std::string_view key, const auto& value, auto /*range*/) {
        text += std::format("{} = {}\n", key, formatValue(value));
    });
    return text;
}

std::expected<ParsedPreset, std::string> parsePreset(const std::string_view text, const Preset& current)
{
    // Preset keys reset to their defaults first (found by key in a default preset: the field list exists
    // once); everything else stays as it is.
    ParsedPreset result {.preset = current, .warnings = {}};
    const Preset defaults {};
    visitFields(result.preset, [&defaults](const std::string_view key, auto& value, auto /*range*/) {
        visitFields(defaults, [&](const std::string_view defaultKey, const auto& defaultValue, auto /*range*/) {
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(value)>,
                                         std::remove_cvref_t<decltype(defaultValue)>>) {
                if (defaultKey == key) {
                    value = defaultValue;
                }
            }
        });
    });

    std::size_t lineNumber {0};
    std::string_view rest {text};
    while (!rest.empty()) {
        ++lineNumber;
        const auto newline {rest.find('\n')};
        const std::string_view line {trim(rest.substr(0, newline))};
        rest = newline == std::string_view::npos ? std::string_view {} : rest.substr(newline + 1);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const auto equals {line.find('=')};
        if (equals == std::string_view::npos) {
            return std::unexpected {std::format("line {}: expected 'key = value', got '{}'", lineNumber, line)};
        }
        const std::string_view key {trim(line.substr(0, equals))};
        const std::string_view valueText {trim(line.substr(equals + 1))};

        bool known {false};
        std::string error {};
        visitFields(result.preset, [&](const std::string_view fieldKey, auto& value, const auto range) {
            if (fieldKey != key) {
                return;
            }
            known = true;
            const auto parsed {parseValue(valueText, value)};
            if (!parsed) {
                error =
                    std::format("line {}: {} must be {}, not '{}'", lineNumber, key, expectedForm(value), valueText);
                return;
            }
            if (!inRange(*parsed, range)) {
                error = std::format("line {}: {} = {} is outside {}", lineNumber, key, valueText, describe(range));
                return;
            }
            value = *parsed;
        });
        if (!error.empty()) {
            return std::unexpected {std::move(error)};
        }
        if (!known) {
            result.warnings.push_back(std::format("line {}: unknown key '{}' skipped", lineNumber, key));
        }
    }
    return result;
}

std::expected<void, std::string> savePreset(const std::filesystem::path& path, const Preset& preset)
{
    std::error_code error {};
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
    }
    std::ofstream file {path};
    file << formatPreset(preset);
    if (!file) {
        return std::unexpected {std::format("{}: could not be written", path.string())};
    }
    return {};
}

std::expected<ParsedPreset, std::string> loadPreset(const std::filesystem::path& path, const Preset& current)
{
    const std::ifstream file {path};
    if (!file) {
        return std::unexpected {std::format("{}: could not be read", path.string())};
    }
    std::ostringstream text {};
    text << file.rdbuf();
    auto parsed {parsePreset(text.str(), current)};
    if (!parsed) {
        return std::unexpected {std::format("{}: {}", path.filename().string(), parsed.error())};
    }
    return parsed;
}

std::vector<std::filesystem::path> findPresets(const std::filesystem::path& directory)
{
    std::vector<std::filesystem::path> found {};
    std::error_code error {};
    for (const auto& entry : std::filesystem::directory_iterator {directory, error}) {
        if (entry.is_regular_file(error) && entry.path().extension() == presetExtension) {
            found.push_back(entry.path());
        }
    }
    std::ranges::sort(found);
    return found;
}

bool isValidPresetName(const std::string_view name)
{
    return !name.empty() && trim(name) == name && std::ranges::all_of(name, [](const char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-'
            || c == '_';
    });
}

} // namespace bh
