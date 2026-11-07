#pragma once
#include <QJsonObject>

class MenuIntegration {
public:
    static QJsonObject install(const QString &menuPath, const QByteArray &fragment, QString *error, const QJsonObject &nativeRow = {});
    static QJsonObject parse(const QString &text, QString *error);
private:
    static QString masked(const QString &text, bool trailingCommas, QString *error);
};
