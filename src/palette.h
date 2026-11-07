#pragma once
#include <QColor>
#include <QJsonObject>
#include <QMap>

struct Palette {
    QMap<QString, QString> colors;
    QString source;
    static Palette create(const QColor &source, const QColor &surface = QColor("#1a1b26"));
    static Palette fromToml(const QByteArray &contents, QString *error);
    QByteArray toml() const;
    QJsonObject report() const;
    static QStringList ansiNames();
    static QStringList chromaticNames();
};
