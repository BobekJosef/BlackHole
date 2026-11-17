#pragma once

#include "gfx/GlObject.hpp"

#include <array>
#include <cstddef>
#include <vector>

namespace bh::gl {

// Measures GPU time of consecutive sections of a frame with GL_TIMESTAMP queries.
// Results are read several frames later, and only once available, so the CPU never waits
// for the GPU. Each frame: beginFrame(), then endSection(i) for every i = 0, 1, ... in order.
class GpuTimer final {
public:
    explicit GpuTimer(std::size_t sectionCount);

    void beginFrame();
    void endSection(std::size_t section);

    // Exponential moving average of the section's duration.
    [[nodiscard]] double milliseconds(std::size_t section) const { return m_milliseconds.at(section); }

private:
    static constexpr std::size_t framesInFlight = 4;

    [[nodiscard]] GLuint query(std::size_t frame, std::size_t timestamp) const;
    void collect(std::size_t frame);

    std::size_t m_sectionCount {};
    // framesInFlight blocks of sectionCount + 1 timestamps: frame start, then the end of each section.
    std::vector<Query> m_queries {};
    std::array<bool, framesInFlight> m_issued {};
    std::size_t m_frame {};
    std::vector<double> m_milliseconds {};
};

} // namespace bh::gl
