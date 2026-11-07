// The portal request pattern follows Omacut (MIT, David Heinemeier Hansson).
#include "portalfilepicker.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QRandomGenerator>

PortalFilePicker::PortalFilePicker(QObject *parent) : FilePicker(parent) {}
PortalFilePicker::~PortalFilePicker() { clear(); }
void PortalFilePicker::openWallpaper() { request(false); }
void PortalFilePicker::openCollection() { request(true); }
bool PortalFilePicker::subscribe(const QString &path) {
    m_path = path;
    return QDBusConnection::sessionBus().connect("org.freedesktop.portal.Desktop", path,
        "org.freedesktop.portal.Request", "Response", this, SLOT(handleResponse(uint,QVariantMap)));
}
void PortalFilePicker::clear() {
    if (!m_path.isEmpty()) {
        QDBusInterface request("org.freedesktop.portal.Desktop", m_path, "org.freedesktop.portal.Request", QDBusConnection::sessionBus());
        if (m_pending) request.asyncCall("Close");
        QDBusConnection::sessionBus().disconnect("org.freedesktop.portal.Desktop", m_path,
            "org.freedesktop.portal.Request", "Response", this, SLOT(handleResponse(uint,QVariantMap)));
    }
    m_path.clear();
    m_pending = false;
    ++m_generation;
}
void PortalFilePicker::request(bool directory) {
    clear();
    const auto bus = QDBusConnection::sessionBus();
    QDBusInterface portal("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.FileChooser", bus);
    if (!portal.isValid()) { emit failed(tr("The desktop portal file chooser is not available.")); return; }
    const QString token = QString("omarchy_aether_%1").arg(QRandomGenerator::global()->generate());
    QString sender = bus.baseService().mid(1);
    sender.replace('.', '_');
    m_directory = directory;
    m_pending = true;
    if (!subscribe(QString("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token))) {
        clear(); emit failed(tr("Could not listen for the file chooser response.")); return;
    }
    const int generation = m_generation;
    const QVariantMap options{{"handle_token", token}, {"modal", true}, {"multiple", false},
                              {"directory", directory}, {"accept_label", tr("Choose")}};
    auto *watcher = new QDBusPendingCallWatcher(portal.asyncCall("OpenFile", QString(),
        directory ? tr("Choose wallpaper collection") : tr("Choose wallpaper"), options), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, generation] {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();
        if (generation != m_generation || !m_pending) return;
        if (reply.isError()) { const QString message = reply.error().message(); clear(); emit failed(tr("File chooser failed: %1").arg(message)); return; }
        if (reply.value().path() != m_path) {
            QDBusConnection::sessionBus().disconnect("org.freedesktop.portal.Desktop", m_path,
                "org.freedesktop.portal.Request", "Response", this, SLOT(handleResponse(uint,QVariantMap)));
            if (!subscribe(reply.value().path())) { clear(); emit failed(tr("Could not listen for the file chooser response.")); }
        }
    });
}
void PortalFilePicker::handleResponse(uint response, const QVariantMap &results) {
    const bool directory = m_directory;
    m_pending = false;
    clear();
    if (response != 0) return;
    const auto uris = results.value("uris").toStringList();
    if (uris.isEmpty() || !QUrl(uris.first()).isLocalFile()) return;
    if (directory) emit collectionSelected(QUrl(uris.first()));
    else emit wallpaperSelected(QUrl(uris.first()));
}
