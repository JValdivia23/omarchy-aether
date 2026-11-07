#pragma once
#include "palette.h"

class ThemeInstaller {
public:
    explicit ThemeInstaller(QString configDirectory = {});
    bool stage(const Palette &palette, const QString &image, QString *error);
    bool apply(const Palette &palette, const QString &image, QString *error);
    QString directory() const;
private:
    QString m_config;
};
