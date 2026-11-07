#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QStringList>

namespace Storage {
QByteArray read(const QString &path, QString *error, qint64 maximum = 16 * 1024 * 1024);
bool write(const QString &path, const QByteArray &data, QString *error);
QJsonObject readJson(const QString &path, QString *error);
bool writeJson(const QString &path, const QJsonObject &data, QString *error);
bool run(const QString &program, const QStringList &arguments, QByteArray *output, QString *error, int timeout = 30000);
QString cacheRoot();
QString configRoot();
QString stateRoot();
}
