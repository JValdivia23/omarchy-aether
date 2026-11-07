#pragma once
#include "palette.h"
#include <QImage>

// An opt-in experiment; the production Palette::create policy stays unchanged.
class HueRotation {
public:
    static QList<QColor> sample(const QImage &image);
    static QList<QColor> sampleFile(const QString &path, QString *error);
    static Palette rotate(const Palette &base, const QList<QColor> &targets, double strength,
                          QString *error, QJsonObject *details = nullptr);
};
