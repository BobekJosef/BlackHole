#pragma once
// The GPU-timed sections of a frame, in submission order (indices into gl::GpuTimer).

#include <array>
#include <cstddef>
#include <string_view>
#include <utility>

namespace bh {

// EXPOSURE: the auto-exposure metering (mipmaps + one work group), when it is on.
enum class GpuPass : std::size_t { TRACE, EXPOSURE, BLOOM, PRESENT, UI };

inline constexpr std::array<std::string_view, 5> gpuPassNames {"Trace", "Exposure", "Bloom", "Present", "UI"};

[[nodiscard]] constexpr std::size_t index(const GpuPass pass)
{
    return std::to_underlying(pass);
}

} // namespace bh
