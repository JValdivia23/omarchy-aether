#include "huerotation.h"
#include "colorutils.h"
#include <QColorSpace>
#include <QImageReader>
#include <QJsonArray>
#include <algorithm>
#include <array>
#include <cmath>

namespace {
double turn(double from, double to) { return std::remainder(to - from, 360.0); }
double wrap(double hue) { return std::fmod(hue + 360, 360); }
const QStringList hueOrder{"red", "yellow", "green", "cyan", "blue", "magenta"};

bool diverse(const Palette &base, const Palette &candidate) {
    if (!candidate.report().value("valid").toBool()) return false;
    for (int i = 0; i < hueOrder.size(); ++i) {
        const QString a = hueOrder[i], b = hueOrder[(i + 1) % hueOrder.size()];
        auto hue = [](const Palette &p, const QString &name) { return Color::toLch(QColor(p.colors.value(name))).h; };
        // Keep the circular order and most of each original gap, not just six unique hex values.
        const double gap = wrap(hue(candidate, b) - hue(candidate, a));
        if (gap < std::max(28.0, .65 * wrap(hue(base, b) - hue(base, a))) || gap > 180) return false;
        for (int j = i + 1; j < hueOrder.size(); ++j) {
            const QString other = hueOrder[j];
            const double before = Color::distance(QColor(base.colors.value(a)), QColor(base.colors.value(other)));
            if (Color::distance(QColor(candidate.colors.value(a)), QColor(candidate.colors.value(other))) < .80 * before) return false;
        }
    }
    for (const auto &name : base.colors.keys()) {
        const auto before = Color::toLch(QColor(base.colors.value(name))), after = Color::toLch(QColor(candidate.colors.value(name)));
        if (std::abs(before.l - after.l) > .025 || after.c < .80 * before.c) return false;
    }
    return true;
}
}

QList<QColor> HueRotation::sample(const QImage &image) {
    if (image.isNull()) return {};
    QImage small = image.scaled(192, 192, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (small.colorSpace().isValid()) small.convertToColorSpace(QColorSpace::SRgb);
    small = small.convertToFormat(QImage::Format_ARGB32);
    struct Bin { double weight = 0, l = 0, a = 0, b = 0; };
    std::array<Bin, 36> bins;
    double total = 0;
    int colorful = 0;
    for (int y = 0; y < small.height(); ++y) {
        for (int x = 0; x < small.width(); ++x) {
            const auto pixel = small.pixelColor(x, y);
            if (pixel.alphaF() < .95) continue;
            const auto color = Color::toLch(pixel);
            if (color.c < .025 || color.l < .14 || color.l > .95) continue;
            ++colorful;
            const double weight = std::min(.16, color.c);
            auto &bin = bins[std::min(35, int(color.h / 10))];
            bin.weight += weight;
            bin.l += color.l * weight;
            bin.a += color.c * std::cos(color.h * M_PI / 180) * weight;
            bin.b += color.c * std::sin(color.h * M_PI / 180) * weight;
            total += weight;
        }
    }
    // Near-neutral images must not acquire an arbitrary hue from quantization noise or a tiny colored speck.
    if (!total || colorful < .01 * small.width() * small.height()) return {};
    struct Peak { double weight; QColor color; };
    QList<Peak> peaks;
    for (int i = 0; i < int(bins.size()); ++i) {
        Bin sum;
        for (int offset = -1; offset <= 1; ++offset) {
            const auto &bin = bins[(i + offset + 36) % 36];
            sum.weight += bin.weight; sum.l += bin.l; sum.a += bin.a; sum.b += bin.b;
        }
        if (sum.weight < .12 * total) continue;
        const double hue = wrap(std::atan2(sum.b, sum.a) * 180 / M_PI);
        peaks.append({sum.weight, Color::fromLch({sum.l / sum.weight, std::hypot(sum.a, sum.b) / sum.weight, hue})});
    }
    std::stable_sort(peaks.begin(), peaks.end(), [](const Peak &a, const Peak &b) { return a.weight > b.weight; });
    QList<QColor> result;
    for (const auto &peak : peaks) {
        if (peak.weight < .20 * peaks.first().weight) break;
        const double hue = Color::toLch(peak.color).h;
        if (std::any_of(result.begin(), result.end(), [&](const QColor &c) { return std::abs(turn(hue, Color::toLch(c).h)) < 45; })) continue;
        result << peak.color;
        if (result.size() == 3) break;
    }
    return result;
}

QList<QColor> HueRotation::sampleFile(const QString &path, QString *error) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (size.isValid()) reader.setScaledSize(size.scaled(192, 192, Qt::KeepAspectRatio));
    const auto image = reader.read();
    if (image.isNull()) { *error = "Cannot sample wallpaper hues: " + reader.errorString(); return {}; }
    return sample(image);
}

Palette HueRotation::rotate(const Palette &base, const QList<QColor> &colors, double strength, QString *error, QJsonObject *details) {
    if (!std::isfinite(strength) || strength < 0 || strength > 1) { *error = "Hue rotation strength must be between 0 and 1."; return {}; }
    if (!base.report().value("valid").toBool()) { *error = "Hue rotation requires a valid base palette."; return {}; }
    QList<QColor> targets;
    for (const auto &color : colors) if (color.isValid() && color.alphaF() >= .95 && Color::toLch(color).c >= .025) targets << color;
    QStringList names = Palette::chromaticNames();
    names << "orange" << "brown";
    QMap<QString, double> wanted;
    QMap<QString, QColor> chosen;
    for (const auto &name : names) {
        const double hue = Color::toLch(QColor(base.colors.value(name))).h;
        double nearest = 180;
        for (const auto &target : targets) {
            const double delta = turn(hue, Color::toLch(target).h);
            if (std::abs(delta) < std::abs(nearest)) { nearest = delta; chosen[name] = target; }
        }
        // Influence is local: an unrelated complementary family remains exactly as it was.
        const double taper = std::clamp((90 - std::abs(nearest)) / 30, 0.0, 1.0);
        wanted[name] = std::clamp(nearest, -24.0, 24.0) * taper;
    }
    const QList<QColor> surfaces{QColor(base.colors.value("background")), QColor(base.colors.value("lighter_background")), QColor(base.colors.value("selection"))};
    auto rotated = [&](double amount) {
        Palette candidate = base;
        for (const auto &name : names) {
            const double delta = wanted.value(name) * amount;
            if (std::abs(delta) < .05) continue;
            QStringList pair{name};
            if (Palette::chromaticNames().contains(name)) pair << "bright_" + name;
            for (const auto &token : pair) {
                auto color = Color::toLch(QColor(base.colors.value(token)));
                color.h += delta;
                candidate.colors[token] = Color::readable(color, surfaces, 4.8).name();
            }
        }
        return candidate;
    };
    Palette result = base;
    double scale = 0;
    // Find the safe full-strength bound first, then use the requested fraction of that bound.
    // Otherwise both a gentle and a strong request can hit the same safety ceiling and look identical.
    for (int step = 100; step > 0; --step) {
        const double amount = step / 100.0;
        if (!diverse(base, rotated(amount))) continue;
        const auto candidate = rotated(amount * strength);
        if (diverse(base, candidate)) { result = candidate; scale = amount; break; }
    }
    QJsonArray targetReport, rotations;
    for (const auto &target : targets) targetReport.append(QJsonObject{{"color", target.name()}, {"hue", Color::toLch(target).h}});
    for (const auto &name : names) {
        const auto before = Color::toLch(QColor(base.colors.value(name))), after = Color::toLch(QColor(result.colors.value(name)));
        QJsonObject row{{"role", name}, {"before", base.colors.value(name)}, {"after", result.colors.value(name)},
                        {"rotation_degrees", turn(before.h, after.h)}, {"requested_degrees", wanted.value(name) * strength}};
        if (chosen.contains(name) && std::abs(wanted.value(name)) > 0) {
            const auto target = chosen.value(name);
            row["target"] = target.name();
            row["distance_before_degrees"] = std::abs(turn(before.h, Color::toLch(target).h));
            row["distance_after_degrees"] = std::abs(turn(after.h, Color::toLch(target).h));
        }
        rotations.append(row);
    }
    if (details) *details = {{"name", "bounded-wallpaper-hue-rotation-v1"}, {"requested_strength", strength},
                            {"safety_scale", scale}, {"effective_strength", scale * strength}, {"retained_baseline", base.colors == result.colors},
                            {"maximum_rotation_degrees", 24 * strength}, {"targets", targetReport}, {"rotations", rotations},
                            {"scope", "Preview/export only. Surfaces, accent, and neutral text are unchanged."}};
    return result;
}
