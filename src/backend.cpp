#include "backend.h"
#include "portalfilepicker.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

Backend::Backend(FilePicker *picker, QObject *parent) : QObject(parent), m_picker(picker ? picker : new PortalFilePicker(this)) {
    m_status = QT_TR_NOOP("Preview first. Your current theme stays unchanged until you apply.");
    connect(m_picker, &FilePicker::wallpaperSelected, this, &Backend::setImage);
    connect(m_picker, &FilePicker::collectionSelected, this, [this](const QUrl &url) {
        start({"index", url.toLocalFile(), "--json"}, "index");
    });
    connect(m_picker, &FilePicker::failed, this, [this](const QString &message) { m_error = message; emit changed(); });
    connect(&m_process, &QProcess::stateChanged, this, [this] { emit changed(); });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) { m_error = m_process.errorString(); emit changed(); }
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        const QString operation = m_operation;
        const auto output = QJsonDocument::fromJson(m_process.readAllStandardOutput()).object();
        if (code != 0 || status != QProcess::NormalExit) {
            m_error = QString::fromUtf8(m_process.readAllStandardError()).trimmed();
            if (m_error.isEmpty()) m_error = tr("The operation failed.");
        } else if (operation == "generate") {
            m_report = output;
            m_status = QT_TR_NOOP("Palette validated. The desktop has not changed.");
        } else if (operation == "pick") {
            const QString path = output.value("wallpaper").toString();
            if (!path.isEmpty()) { setImage(QUrl::fromLocalFile(path)); primaryAction(); return; }
            m_status = QT_TR_NOOP("Selection canceled. Nothing changed.");
        } else if (operation == "index") {
            start({"prepare", "--preload", "--json"}, "prepare");
            return;
        } else if (operation == "prepare") {
            m_status = QT_TR_NOOP("Wallpaper previews are ready. Open the native picker to browse.");
        } else if (operation == "apply") {
            m_status = QT_TR_NOOP("Aether applied through Omarchy.");
        }
        emit changed();
    });
}
Backend::~Backend() {
    if (m_process.state() != QProcess::NotRunning) { m_process.kill(); m_process.waitForFinished(1000); }
}
QString Backend::fileName() const { return QFileInfo(m_image.toLocalFile()).fileName(); }
QVariantList Backend::colors() const {
    QVariantList values;
    const auto colors = m_report.value("colors").toObject();
    for (const auto &name : Palette::ansiNames()) {
        if (colors.contains(name)) values << QVariantMap{{"name", name}, {"color", colors.value(name).toString()}};
    }
    return values;
}
void Backend::setImage(const QUrl &url) {
    if (busy()) return;
    m_image = url;
    m_report = {};
    m_error.clear();
    emit changed();
}
void Backend::setPreview(const Palette &palette) { m_report = palette.report(); emit changed(); }
void Backend::chooseWallpaper() { if (!busy()) m_picker->openWallpaper(); }
void Backend::chooseCollection() { if (!busy()) m_picker->openCollection(); }
void Backend::start(const QStringList &arguments, const QString &operation) {
    if (busy()) return;
    m_error.clear();
    m_operation = operation;
    m_status = QT_TR_NOOP("Working…");
    m_process.start(QCoreApplication::applicationFilePath(), arguments);
    emit changed();
}
void Backend::primaryAction() {
    if (m_image.isEmpty()) { chooseWallpaper(); return; }
    start({"generate", "--image", m_image.toLocalFile(), "--json"}, "generate");
}
void Backend::nativePicker() { start({"pick", "--json"}, "pick"); }
void Backend::applyTheme() {
    if (ready() && !m_image.isEmpty()) start({"apply", "--image", m_image.toLocalFile(), "--yes", "--json"}, "apply");
}
