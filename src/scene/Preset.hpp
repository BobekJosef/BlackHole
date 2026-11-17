#pragma once
// Presets: a scene and a camera placement saved as a small text file, one "key = value" per line, lines
// starting with # are comments:
//
//   spin = 0.9
//   disk.peak_temperature = 6500
//   sky.background = image
//   sky.image = eso0932a.jpg
//   camera.polar = 84
//
// Keys a file leaves out take their default values, so hand-written presets can be short. Machine and
// session preferences (VSync, fullscreen, render scale, integrator accuracy, debug views, screenshot
// size) are not part of a preset: loading one keeps the current ones. No OpenGL, unit-tested.

#include "scene/OrbitCamera.hpp"
#include "scene/SceneSettings.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace bh {

struct Preset {
    SceneSettings settings {};
    CameraPlacement camera {};
};

inline constexpr std::string_view presetExtension {".preset"};

[[nodiscard]] std::string formatPreset(const Preset& preset);

struct [[nodiscard]] ParsedPreset {
    Preset preset {};
    std::vector<std::string> warnings {}; // e.g. unknown keys, which are skipped
};

// Reads `text` on top of `current`: preset keys the text has replace the current values, preset keys it
// lacks reset to their defaults, everything that is not part of a preset stays. Fails on a malformed
// line or a value that is out of range, naming the line.
[[nodiscard]] std::expected<ParsedPreset, std::string> parsePreset(std::string_view text, const Preset& current);

[[nodiscard]] std::expected<void, std::string> savePreset(const std::filesystem::path& path, const Preset& preset);
[[nodiscard]] std::expected<ParsedPreset, std::string> loadPreset(const std::filesystem::path& path,
                                                                  const Preset& current);

// The presets (*.preset) directly in `directory`, sorted by name.
[[nodiscard]] std::vector<std::filesystem::path> findPresets(const std::filesystem::path& directory);

// A preset file name must be a plain name: letters, digits, space, '-', '_' (no path, no extension).
[[nodiscard]] bool isValidPresetName(std::string_view name);

} // namespace bh
