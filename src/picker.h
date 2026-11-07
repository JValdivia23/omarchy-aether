#pragma once
#include <QJsonObject>

class Picker {
public:
    explicit Picker(QString cache = {}, QString shell = "omarchy-shell", int heartbeatMilliseconds = 5000);
    bool preload(const QJsonObject &batch, const QString &current, QString *error);
    QString select(const QJsonObject &batch, const QString &current, QString *error);
    static QByteArray rows(const QJsonObject &batch);
private:
    QString selectedAlias(const QJsonObject &batch, const QString &current) const;
    QString m_cache, m_shell;
    int m_heartbeat;
};
