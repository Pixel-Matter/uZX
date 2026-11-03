#pragma once
#include <JuceHeader.h>

#include <cmath>

namespace MoTool::Oklch {

//==============================================================================
/** A colour in OKLCH (Björn Ottosson's OKLab in polar form): perceptually
    uniform, so equal L means equal lightness and equal C equal colourfulness
    across hues — unlike HSV/HSL.
    L: 0 (black)..1 (white), C: 0 (grey)..~0.4, H: hue in degrees. */
struct Lch {
    float L = 0.0f;
    float C = 0.0f;
    float H = 0.0f;
};

namespace detail {

inline float toLinear(float x) {
    return x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f);
}

inline float fromLinear(float x) {
    return x <= 0.0031308f ? 12.92f * x : 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
}

struct Rgb { float r, g, b; };

/** OKLCH to linear sRGB, unclamped: components outside 0..1 are out of gamut. */
inline Rgb toLinearRgb(Lch c) {
    const float h = juce::degreesToRadians(c.H);
    const float a = c.C * std::cos(h);
    const float b = c.C * std::sin(h);

    const float l_ = c.L + 0.3963377774f * a + 0.2158037573f * b;
    const float m_ = c.L - 0.1055613458f * a - 0.0638541728f * b;
    const float s_ = c.L - 0.0894841775f * a - 1.2914855480f * b;
    const float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;

    return {
        +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
        -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
        -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s,
    };
}

inline bool inGamut(Rgb c) {
    constexpr float eps = 1e-4f;
    return c.r >= -eps && c.r <= 1 + eps && c.g >= -eps && c.g <= 1 + eps && c.b >= -eps && c.b <= 1 + eps;
}

} // namespace detail

/** Largest chroma displayable in sRGB at this lightness and hue. */
inline float maxChroma(float L, float H) {
    float lo = 0.0f, hi = 0.4f;
    for (int i = 0; i < 20; ++i) {
        const float mid = (lo + hi) * 0.5f;
        (detail::inGamut(detail::toLinearRgb({L, mid, H})) ? lo : hi) = mid;
    }
    return lo;
}

/** Converts to an sRGB Colour. Out-of-gamut chroma is reduced to the sRGB
    boundary, keeping L and H, so lightness never shifts. */
inline Colour toColour(Lch c, float alpha = 1.0f) {
    c.C = juce::jmin(c.C, maxChroma(c.L, c.H));
    const auto rgb = detail::toLinearRgb(c);
    auto channel = [] (float x) { return juce::jlimit(0.0f, 1.0f, detail::fromLinear(x)); };
    return Colour::fromFloatRGBA(channel(rgb.r), channel(rgb.g), channel(rgb.b), alpha);
}

inline Lch fromColour(Colour colour) {
    const float r = detail::toLinear(colour.getFloatRed());
    const float g = detail::toLinear(colour.getFloatGreen());
    const float b = detail::toLinear(colour.getFloatBlue());

    const float l = std::cbrt(0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b);
    const float m = std::cbrt(0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b);
    const float s = std::cbrt(0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b);

    const float L  = 0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s;
    const float a  = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s;
    const float bb = 0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s;

    float H = juce::radiansToDegrees(std::atan2(bb, a));
    if (H < 0.0f)
        H += 360.0f;
    return {L, std::hypot(a, bb), H};
}

} // namespace MoTool::Oklch
