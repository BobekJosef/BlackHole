#pragma once
// Camera orbiting the black hole at the origin. World frame: spin axis +z, equatorial
// (disk) plane z = 0, distances in units of M. Spherical angles are measured like
// Boyer-Lindquist theta (from +z) and phi.

#include <glm/glm.hpp>

namespace bh {

// Where an orbit camera sits, in the units a person edits (presets).
struct CameraPlacement {
    float azimuthDegrees {0.0F};
    float polarDegrees {80.0F}; // from +z (the spin axis)
    float distance {30.0F};     // M
    // Look-around: where the view points relative to the hole (or the fall direction).
    float lookYawDegrees {0.0F};   // + to the right
    float lookPitchDegrees {0.0F}; // + up
    bool operator==(const CameraPlacement&) const = default;
};

class OrbitCamera final {
public:
    // Turns by a cursor movement in pixels: x changes phi, y changes theta.
    void orbit(glm::vec2 cursorDelta);
    // Multiplicative zoom; positive steps move closer.
    void zoom(double steps);
    // Carries the camera around the spin axis (an orbiting observer).
    void turn(float azimuthRadians);
    // Turns the view by a cursor movement in pixels (mouse-look: right turns right, up looks up), on top
    // of the direction the camera mode points in.
    void look(glm::vec2 cursorDelta);
    void recenterLook();
    [[nodiscard]] bool isLookCentered() const { return m_lookYaw == 0.0F && m_lookPitch == 0.0F; }
    // `base` (columns right, up, forward, as basis()) turned by the look-around angles.
    [[nodiscard]] glm::mat3 applyLook(const glm::mat3& base) const;

    [[nodiscard]] CameraPlacement placement() const;
    // Moves the camera there, clamped to the distance range and off the poles.
    void place(const CameraPlacement& placement);

    [[nodiscard]] float distance() const { return m_distance; }
    [[nodiscard]] float verticalFovDegrees() const { return m_fovDegrees; }
    void setVerticalFovDegrees(float degrees);

    [[nodiscard]] glm::vec3 position() const;
    // Camera-to-world rotation; columns are right, up and forward.
    [[nodiscard]] glm::mat3 basis() const { return lookAlong(-glm::normalize(position())); }

    // Basis looking along unit `forward` with +z up: columns right, up, forward.
    [[nodiscard]] static glm::mat3 lookAlong(const glm::vec3& forward);

private:
    float m_distance {30.0F};
    float m_azimuth {0.0F};
    float m_polar {1.396F}; // 80 degrees: a nearly edge-on view of the disk
    float m_fovDegrees {60.0F};
    float m_lookYaw {0.0F};   // radians, wrapped to (-pi, pi]
    float m_lookPitch {0.0F}; // radians, within +-maxLookPitch
};

} // namespace bh
