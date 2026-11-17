#pragma once
// Validation of the GPU tracer against the CPU reference (blackhole --validate): the same camera rays
// traced by trace.comp in float and by kerr::traceRay in double, compared pixel by pixel. Both run the same
// algorithm (step rule, capture and escape radii, disk crossing), so the differences are float rounding,
// amplified only where the geometry is chaotic (next to the photon ring, at disk edges). Catches drift
// between the GLSL copy and the C++ original. No OpenGL here: the GPU result comes in as data.

#include "physics/Geodesic.hpp"
#include "physics/KerrSchild.hpp"

#include <glm/glm.hpp>

#include <span>
#include <string>

namespace bh {

// What the GPU traced, in the reference's terms (double precision of the same float inputs).
struct ReferenceSetup {
    double spin {};
    kerr::ThreeVector cameraPosition {};
    kerr::Tetrad cameraTetrad {};
    double tanHalfFov {};
    int width {};
    int height {};
    kerr::TraceSettings trace {};
};

struct [[nodiscard]] ValidationResult {
    bool passed {};
    std::string report {}; // human-readable, several lines
};

// `gpu`: width x height pixels, rows from the bottom, as BlackHoleRenderer::traceForValidation returns them.
[[nodiscard]] ValidationResult compareWithReference(std::string_view name, const ReferenceSetup& setup,
                                                    std::span<const glm::vec4> gpu);

} // namespace bh
