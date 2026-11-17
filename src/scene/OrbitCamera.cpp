#include "scene/OrbitCamera.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace bh {

namespace {

constexpr float radiansPerPixel = 0.005F;
constexpr float zoomPerStep = 0.1F;
constexpr float minDistance = 3.0F;
constexpr float maxDistance = 1000.0F;
// Keeps the camera off the axis, where the azimuth (and the "right" vector) is undefined.
constexpr float poleMargin = 0.01F;

} // namespace

void OrbitCamera::orbit(const glm::vec2 cursorDelta)
{
    m_azimuth = std::remainder(m_azimuth - cursorDelta.x * radiansPerPixel, 2.0F * std::numbers::pi_v<float>);
    m_polar = std::clamp(m_polar - cursorDelta.y * radiansPerPixel, poleMargin, std::numbers::pi_v<float> - poleMargin);
}

void OrbitCamera::zoom(const double steps)
{
    m_distance = std::clamp(m_distance * std::exp(-zoomPerStep * static_cast<float>(steps)), minDistance, maxDistance);
}

void OrbitCamera::setVerticalFovDegrees(const float degrees)
{
    m_fovDegrees = std::clamp(degrees, 10.0F, 140.0F);
}

glm::vec3 OrbitCamera::position() const
{
    const float sinPolar {std::sin(m_polar)};
    return m_distance * glm::vec3 {sinPolar * std::cos(m_azimuth), sinPolar * std::sin(m_azimuth), std::cos(m_polar)};
}

glm::mat3 OrbitCamera::basis() const
{
    const glm::vec3 forward {-glm::normalize(position())};
    const glm::vec3 right {glm::normalize(glm::cross(forward, glm::vec3 {0.0F, 0.0F, 1.0F}))};
    const glm::vec3 up {glm::cross(right, forward)};
    return glm::mat3 {right, up, forward};
}

} // namespace bh
