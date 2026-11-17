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
// Looking straight up or down would make "right" undefined.
constexpr float maxLookPitch = glm::radians(89.0F);

} // namespace

void OrbitCamera::orbit(const glm::vec2 cursorDelta)
{
    m_azimuth = std::remainder(m_azimuth - cursorDelta.x * radiansPerPixel, 2.0F * std::numbers::pi_v<float>);
    m_polar = std::clamp(m_polar - cursorDelta.y * radiansPerPixel, poleMargin, std::numbers::pi_v<float> - poleMargin);
}

void OrbitCamera::turn(const float azimuthRadians)
{
    m_azimuth = std::remainder(m_azimuth + azimuthRadians, 2.0F * std::numbers::pi_v<float>);
}

void OrbitCamera::look(const glm::vec2 cursorDelta)
{
    m_lookYaw = std::remainder(m_lookYaw + cursorDelta.x * radiansPerPixel, 2.0F * std::numbers::pi_v<float>);
    m_lookPitch = std::clamp(m_lookPitch - cursorDelta.y * radiansPerPixel, -maxLookPitch, maxLookPitch);
}

void OrbitCamera::recenterLook()
{
    m_lookYaw = 0.0F;
    m_lookPitch = 0.0F;
}

glm::mat3 OrbitCamera::applyLook(const glm::mat3& base) const
{
    // The turned axes in the base frame (x right, y up, z forward): yaw about up, then pitch.
    const float cosYaw {std::cos(m_lookYaw)};
    const float sinYaw {std::sin(m_lookYaw)};
    const float cosPitch {std::cos(m_lookPitch)};
    const float sinPitch {std::sin(m_lookPitch)};
    const glm::vec3 forward {sinYaw * cosPitch, sinPitch, cosYaw * cosPitch};
    const glm::vec3 right {cosYaw, 0.0F, -sinYaw};
    const glm::vec3 up {glm::cross(forward, right)};
    return glm::mat3 {base * right, base * up, base * forward};
}

CameraPlacement OrbitCamera::placement() const
{
    return CameraPlacement {.azimuthDegrees = glm::degrees(m_azimuth),
                            .polarDegrees = glm::degrees(m_polar),
                            .distance = m_distance,
                            .lookYawDegrees = glm::degrees(m_lookYaw),
                            .lookPitchDegrees = glm::degrees(m_lookPitch)};
}

void OrbitCamera::place(const CameraPlacement& placement)
{
    m_azimuth = std::remainder(glm::radians(placement.azimuthDegrees), 2.0F * std::numbers::pi_v<float>);
    m_polar = std::clamp(glm::radians(placement.polarDegrees), poleMargin, std::numbers::pi_v<float> - poleMargin);
    m_distance = std::clamp(placement.distance, minDistance, maxDistance);
    m_lookYaw = std::remainder(glm::radians(placement.lookYawDegrees), 2.0F * std::numbers::pi_v<float>);
    m_lookPitch = std::clamp(glm::radians(placement.lookPitchDegrees), -maxLookPitch, maxLookPitch);
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

glm::mat3 OrbitCamera::lookAlong(const glm::vec3& forward)
{
    const glm::vec3 right {glm::normalize(glm::cross(forward, glm::vec3 {0.0F, 0.0F, 1.0F}))};
    const glm::vec3 up {glm::cross(right, forward)};
    return glm::mat3 {right, up, forward};
}

} // namespace bh
