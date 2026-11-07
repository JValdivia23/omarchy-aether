#pragma once
#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QTimer>

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(QString background READ background NOTIFY changed)
    Q_PROPERTY(QString foreground READ foreground NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)
public:
    explicit Theme(QString currentDirectory = {}, QObject *parent = nullptr, QString hyprctl = {});
    ~Theme() override;
    QString accent() const { return m_accent; }
    QString accentForeground() const;
    QString background() const { return m_background; }
    QString foreground() const { return m_foreground; }
    int radius() const { return m_radius; }
signals:
    void changed();
private:
    void reload();
    void refreshRounding();
    QString m_directory, m_hyprctl;
    QString m_accent = "#FFD60A", m_background = "#1a1b26", m_foreground = "#c0caf5";
    int m_radius = 0;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce, m_poll, m_timeout;
    QProcess m_process;
};
