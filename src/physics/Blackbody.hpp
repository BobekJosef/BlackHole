#pragma once
// Colour of thermal radiation: Planck's law integrated against the CIE 1931 2-degree colour matching
// functions, converted to linear sRGB (Rec. 709 primaries, D65 white).
//
// Relative units: the luminance Y of a 6500 K black body is 1. Radiance at other temperatures is on
// the same scale (it grows ~T^4 bolometrically, ~T in the visible band at high T).

namespace bh {

struct LinearRgb {
    double r {};
    double g {};
    double b {};
};

// Components outside the sRGB gamut (deep red below ~1900 K) are clipped to 0.
[[nodiscard]] LinearRgb blackbodyRgb(double kelvin);

// CIE luminance Y on the same scale.
[[nodiscard]] double blackbodyLuminance(double kelvin);

} // namespace bh
