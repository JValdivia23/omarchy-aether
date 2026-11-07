#pragma once
#include "filepicker.h"
#include "palette.h"
#include <QProcess>
#include <QVariantList>
#include <QVariantMap>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl image READ image NOTIFY changed)
    Q_PROPERTY(QString fileName READ fileName NOTIFY changed)
    Q_PROPERTY(QVariantList colors READ colors NOTIFY changed)
    Q_PROPERTY(QVariantMap metrics READ metrics NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit Backend(FilePicker *picker = nullptr, QObject *parent = nullptr);
    ~Backend() override;
    QUrl image() const { return m_image; }
    QString fileName() const;
    QVariantList colors() const;
    QVariantMap metrics() const { return m_report.value("metrics").toObject().toVariantMap(); }
    bool ready() const { return m_report.value("valid").toBool(); }
    bool busy() const { return m_process.state() != QProcess::NotRunning; }
    QString error() const { return m_error; }
    QString status() const { return tr(m_status.toUtf8().constData()); }
    void refreshLanguage() { emit changed(); }
    Q_INVOKABLE void chooseWallpaper();
    Q_INVOKABLE void chooseCollection();
    Q_INVOKABLE void primaryAction();
    Q_INVOKABLE void nativePicker();
    Q_INVOKABLE void applyTheme();
    void setImage(const QUrl &url);
    void setPreview(const Palette &palette);
signals:
    void changed();
private:
    void start(const QStringList &arguments, const QString &operation);
    FilePicker *m_picker;
    QProcess m_process;
    QUrl m_image;
    QJsonObject m_report;
    QString m_error, m_status, m_operation;
};
