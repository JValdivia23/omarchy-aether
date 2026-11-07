#pragma once
#include "palette.h"

class Generator {
public:
    explicit Generator(QString cacheDirectory = {}, QString executable = "matugen");
    Palette generate(const QString &image, QString *error, int sourceIndex = 0);
    static bool extract(const QJsonObject &json, QColor *source, QColor *surface, QString *error);
private:
    QString m_cache, m_executable;
};
