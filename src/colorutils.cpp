#include "colorutils.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace {
double linear(double v) { return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4); }
double encoded(double v) { return v <= .0031308 ? 12.92 * v : 1.055 * std::pow(v, 1.0 / 2.4) - .055; }
std::array<double, 3> lab(const QColor &color) {
    const double r = linear(color.redF()), g = linear(color.greenF()), b = linear(color.blueF());
    const double l = std::cbrt(.4122214708*r + .5363325363*g + .0514459929*b);
    const double m = std::cbrt(.2119034982*r + .6806995451*g + .1073969566*b);
    const double s = std::cbrt(.0883024619*r + .2817188376*g + .6299787005*b);
    return {.2104542553*l + .793617785*m - .0040720468*s,
            1.9779984951*l - 2.428592205*m + .4505937099*s,
            .0259040371*l + .7827717662*m - .808675766*s};
}
std::array<double, 3> rgb(Color::Lch color) {
    const double a = color.c * std::cos(color.h * M_PI / 180), b = color.c * std::sin(color.h * M_PI / 180);
    const double l = std::pow(color.l + .3963377774*a + .2158037573*b, 3);
    const double m = std::pow(color.l - .1055613458*a - .0638541728*b, 3);
    const double s = std::pow(color.l - .0894841775*a - 1.291485548*b, 3);
    return {4.0767416621*l - 3.3077115913*m + .2309699292*s,
            -1.2684380046*l + 2.6097574011*m - .3413193965*s,
            -.0041960863*l - .7034186147*m + 1.707614701*s};
}
bool inGamut(const std::array<double, 3> &v) {
    return std::all_of(v.begin(), v.end(), [](double x) { return x >= -1e-7 && x <= 1.0000001; });
}
}

double Color::luminance(const QColor &c) {
    return .2126 * linear(c.redF()) + .7152 * linear(c.greenF()) + .0722 * linear(c.blueF());
}
double Color::contrast(const QColor &a, const QColor &b) {
    const double x = luminance(a), y = luminance(b);
    return (std::max(x, y) + .05) / (std::min(x, y) + .05);
}
Color::Lch Color::toLch(const QColor &c) {
    const auto v = lab(c);
    return {v[0], std::hypot(v[1], v[2]), std::fmod(std::atan2(v[2], v[1]) * 180 / M_PI + 360, 360)};
}
QColor Color::fromLch(Lch color) {
    // Reduce chroma at the gamut boundary instead of clipping channels and changing the hue.
    double low = 0, high = color.c;
    auto values = rgb(color);
    if (!inGamut(values)) {
        for (int i = 0; i < 24; ++i) {
            const double middle = (low + high) / 2;
            auto candidate = color;
            candidate.c = middle;
            if (inGamut(rgb(candidate))) low = middle; else high = middle;
        }
        color.c = low;
        values = rgb(color);
    }
    return QColor::fromRgbF(std::clamp(encoded(std::max(0.0, values[0])), 0.0, 1.0),
                            std::clamp(encoded(std::max(0.0, values[1])), 0.0, 1.0),
                            std::clamp(encoded(std::max(0.0, values[2])), 0.0, 1.0)).toRgb();
}
double Color::distance(const QColor &a, const QColor &b) {
    const auto x = lab(a), y = lab(b);
    return 100 * std::sqrt(std::pow(x[0]-y[0], 2) + std::pow(x[1]-y[1], 2) + std::pow(x[2]-y[2], 2));
}
QColor Color::readable(Lch color, const QList<QColor> &surfaces, double minimum) {
    for (int step = 0; step < 1000; ++step) {
        const QColor result(fromLch(color).name());
        if (std::all_of(surfaces.begin(), surfaces.end(), [&](const QColor &bg) {
                return contrast(result, bg) >= minimum;
            })) return result;
        color.l = std::min(.99, color.l + .001);
    }
    return QColor("#ffffff");
}
