#pragma once
#include <QColor>

namespace Color {
struct Lch { double l, c, h; };
double luminance(const QColor &color);
double contrast(const QColor &a, const QColor &b);
Lch toLch(const QColor &color);
QColor fromLch(Lch color);
double distance(const QColor &a, const QColor &b);
QColor readable(Lch color, const QList<QColor> &surfaces, double minimum);
}
