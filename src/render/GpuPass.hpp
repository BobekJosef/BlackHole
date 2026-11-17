#pragma once
// The GPU-timed sections of a frame, in submission order (indices into gl::GpuTimer).

#include <array>
#include <cstddef>
#include <string_view>
#include <utility>

namespace bh {

enum class GpuPass : std::size_t { TRACE, BLOOM, PRESENT, UI };

inline constexpr std::array<std::string_view, 4> gpuPassNames {"Trace", "Bloom", "Present", "UI"};

[[nodiscard]] constexpr std::size_t index(const GpuPass pass)
{
    return std::to_underlying(pass);
}

} // namespace bh
