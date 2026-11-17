#pragma once

#include "physics/Kerr.hpp"

#include <vector>

namespace bh {

// Draws the analytic shadow edge (kerr::shadowOutline) over the image, for comparison with the
// traced shadow. The outline is exact for a distant observer; a camera at Boyer-Lindquist radius
// `cameraRadius` sees an impact parameter b at sin(psi) = b sqrt(1 - 2/r) / r (exact for a = 0,
// accurate to O(a^2/r^2) otherwise).
void drawShadowOutline(const std::vector<kerr::ShadowPoint>& outline, double cameraRadius, double tanHalfFov);

} // namespace bh
