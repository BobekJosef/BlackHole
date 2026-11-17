#pragma once
// Camera orbiting the black hole at the origin. World frame: spin axis +z, equatorial
// (disk) plane z = 0, distances in units of M. Spherical angles are measured like
// Boyer-Lindquist theta (from +z) and phi.

#include <glm/glm.hpp>

namespace bh {

class OrbitCamera final {
public:
    // Turns by a cursor movement in pixels: x changes phi, y changes theta.
    void orbit(glm::vec2 cursorDelta);
    // Multiplicative zoom; positive steps move closer.
    void zoom(double steps);

    [[nodiscard]] float distance() const { return m_distance; }
    [[nodiscard]] float polarAngle() const { return m_polar; }
    [[nodiscard]] float verticalFovDegrees() const { return m_fovDegrees; }
    void setVerticalFovDegrees(float degrees);

    [[nodiscard]] glm::vec3 position() const;
    // Camera-to-world rotation; columns are right, up and forward.
    [[nodiscard]] glm::mat3 basis() const;

private:
    float m_distance {30.0F};
    float m_azimuth {0.0F};
    float m_polar {1.396F}; // 80 degrees: a nearly edge-on view of the disk
    float m_fovDegrees {60.0F};
};

} // namespace bh
