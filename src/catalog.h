#pragma once
#include <QJsonObject>
#include <QStringList>

class Catalog {
public:
    explicit Catalog(QString cacheDirectory = {});
    bool index(const QStringList &directories, QString *error);
    QJsonObject load(QString *error) const;
    QJsonObject prepare(int count, const QString &current, QString *error);
    QJsonObject ready(QString *error) const;
    bool warm(int limit, QString *error);
    bool thumbnail(const QJsonObject &entry, QString *error) const;
    static QJsonObject entry(const QString &path, const QString &cache);
    QString root() const { return m_cache; }
private:
    QString m_cache;
};
