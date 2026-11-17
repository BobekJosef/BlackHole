#include "gfx/GpuTimer.hpp"

namespace bh::gl {

namespace {

// Weight of the newest sample in the moving average.
constexpr double smoothing = 0.05;
constexpr double nanosecondsPerMillisecond = 1.0e6;

} // namespace

GpuTimer::GpuTimer(const std::size_t sectionCount)
    : m_sectionCount {sectionCount}
    , m_milliseconds(sectionCount, 0.0)
{
    const std::size_t count {framesInFlight * (sectionCount + 1)};
    m_queries.reserve(count);
    for (std::size_t i {0}; i < count; ++i) {
        m_queries.push_back(createQuery());
    }
}

GLuint GpuTimer::query(const std::size_t frame, const std::size_t timestamp) const
{
    return m_queries[frame * (m_sectionCount + 1) + timestamp].get();
}

void GpuTimer::collect(const std::size_t frame)
{
    GLint available {};
    glGetQueryObjectiv(query(frame, m_sectionCount), GL_QUERY_RESULT_AVAILABLE, &available);
    if (available != GL_TRUE) {
        return; // the GPU is more than framesInFlight frames behind: drop this sample rather than stall
    }
    GLuint64 previous {};
    glGetQueryObjectui64v(query(frame, 0), GL_QUERY_RESULT, &previous);
    for (std::size_t section {0}; section < m_sectionCount; ++section) {
        GLuint64 end {};
        glGetQueryObjectui64v(query(frame, section + 1), GL_QUERY_RESULT, &end);
        const double sample {static_cast<double>(end - previous) / nanosecondsPerMillisecond};
        m_milliseconds[section] += smoothing * (sample - m_milliseconds[section]);
        previous = end;
    }
}

void GpuTimer::beginFrame()
{
    m_frame = (m_frame + 1) % framesInFlight;
    if (m_issued[m_frame]) {
        collect(m_frame);
    }
    glQueryCounter(query(m_frame, 0), GL_TIMESTAMP);
    m_issued[m_frame] = true;
}

void GpuTimer::endSection(const std::size_t section)
{
    glQueryCounter(query(m_frame, section + 1), GL_TIMESTAMP);
}

} // namespace bh::gl
