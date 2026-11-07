// Theme watching follows Omacut and the Omarchy app templates (MIT, David Heinemeier Hansson).
#include "theme.h"
#include "colorutils.h"
#include "storage.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

Theme::Theme(QString directory, QObject *parent, QString hyprctl)
    : QObject(parent), m_directory(directory.isEmpty() ? Storage::stateRoot() : directory) {
    m_hyprctl = !hyprctl.isEmpty() ? hyprctl : directory.isEmpty() && !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")
        ? QStandardPaths::findExecutable("hyprctl") : QString();
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Theme::reload);
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(500);
    connect(&m_timeout, &QTimer::timeout, &m_process, &QProcess::kill);
    connect(&m_process, &QProcess::errorOccurred, this, [this] { m_timeout.stop(); });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_timeout.stop();
        const auto value = QJsonDocument::fromJson(m_process.readAllStandardOutput()).object().value("int");
        if (code == 0 && status == QProcess::NormalExit && value.isDouble() && value.toInt(-1) >= 0 && value.toInt() != m_radius) {
            m_radius = value.toInt();
            emit changed();
        }
    });
    m_poll.setInterval(1000);
    connect(&m_poll, &QTimer::timeout, this, &Theme::refreshRounding);
    if (!m_hyprctl.isEmpty()) m_poll.start();
    reload();
}
Theme::~Theme() {
    if (m_process.state() != QProcess::NotRunning) { m_process.kill(); m_process.waitForFinished(1000); }
}
QString Theme::accentForeground() const {
    return Color::contrast(QColor(m_accent), Qt::black) >= Color::contrast(QColor(m_accent), Qt::white) ? "black" : "white";
}
void Theme::refreshRounding() {
    if (m_hyprctl.isEmpty() || m_process.state() != QProcess::NotRunning) return;
    m_process.start(m_hyprctl, {"-j", "getoption", "decoration:rounding"});
    m_timeout.start();
}
void Theme::reload() {
    refreshRounding();
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty()) m_watcher.removePaths(paths);
    QString ancestor = m_directory;
    while (!QFileInfo::exists(ancestor) && ancestor != "/") ancestor = QFileInfo(ancestor).absolutePath();
    QStringList candidates{ancestor, QFileInfo(ancestor).absolutePath(), m_directory, m_directory + "/theme", m_directory + "/theme/colors.toml"};
    candidates.removeDuplicates();
    for (const auto &path : candidates) if (QFileInfo::exists(path)) m_watcher.addPath(path);
    QFile file(m_directory + "/theme/colors.toml");
    const QString text = file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
    auto token = [&](const QString &name, const QString &fallback) {
        const QRegularExpression expression("^\\s*" + name + "\\s*=\\s*[\"'](#[0-9a-fA-F]{6})[\"']", QRegularExpression::MultilineOption);
        const auto match = expression.match(text);
        return match.hasMatch() ? match.captured(1) : fallback;
    };
    const QString accent = token("accent", "#FFD60A"), background = token("background", "#1a1b26"), foreground = token("foreground", "#c0caf5");
    if (accent != m_accent || background != m_background || foreground != m_foreground) {
        m_accent = accent; m_background = background; m_foreground = foreground;
        emit changed();
    }
}
