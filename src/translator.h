#pragma once
#include <QFileSystemWatcher>
#include <QHash>
#include <QJsonValue>
#include <QLocale>
#include <QTimer>
#include <QTranslator>

class Translator : public QTranslator {
    Q_OBJECT
    Q_PROPERTY(QString language READ language NOTIFY changed)
public:
    explicit Translator(QString languageFile = {}, QString catalogs = ":/i18n", QObject *parent = nullptr);
    QString language() const { return m_language; }
    bool isEmpty() const override { return m_strings.isEmpty(); }
    QString translate(const char *, const char *text, const char * = nullptr, int n = -1) const override;
    static QString pluralCategory(const QString &language, long long n);
signals:
    void changed();
private:
    void reload();
    QString m_file, m_catalogs, m_language;
    QHash<QString, QJsonValue> m_strings;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
