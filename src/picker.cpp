#include "picker.h"
#include "storage.h"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QThread>

Picker::Picker(QString cache, QString shell, int heartbeat)
    : m_cache(cache.isEmpty() ? Storage::cacheRoot() : cache), m_shell(shell), m_heartbeat(qMax(1, heartbeat)) {}

QByteArray Picker::rows(const QJsonObject &batch) {
    QByteArray result;
    for (const auto &value : batch.value("images").toArray()) {
        const auto image = value.toObject();
        result += image.value("alias").toString().toUtf8() + '\t' + image.value("thumbnail").toString().toUtf8() + '\n';
    }
    return result;
}
QString Picker::selectedAlias(const QJsonObject &batch, const QString &current) const {
    const QString canonical = QFileInfo(current).canonicalFilePath();
    for (const auto &value : batch.value("images").toArray())
        if (value.toObject().value("path").toString() == canonical)
            return value.toObject().value("alias").toString();
    return {};
}
bool Picker::preload(const QJsonObject &batch, const QString &current, QString *error) {
    QByteArray output;
    if (!Storage::run(m_shell, {"image-selector", "preload", QString::fromLatin1(rows(batch).toBase64()),
                               selectedAlias(batch, current), "false", "false"}, &output, error, 5000)) return false;
    if (output.trimmed() != "ok") { *error = "The Omarchy carousel did not accept the preload request."; return false; }
    return true;
}
QString Picker::select(const QJsonObject &batch, const QString &current, QString *error) {
    QDir().mkpath(m_cache + "/requests");
    QTemporaryDir request(m_cache + "/requests/picker-XXXXXX");
    if (!request.isValid()) { *error = "Cannot create picker request files."; return {}; }
    const QString selection = request.filePath("selected"), done = request.filePath("done");
    QByteArray output;
    if (!Storage::run(m_shell, {"image-selector", "open", "", QString::fromLatin1(rows(batch).toBase64()),
                               selectedAlias(batch, current), selection, done, "false", "false"}, &output, error, 5000)) return {};
    if (output.trimmed() != "ok") { *error = "The Omarchy carousel did not accept the request."; return {}; }
    QElapsedTimer heartbeat;
    heartbeat.start();
    while (!QFileInfo::exists(done)) {
        QCoreApplication::processEvents();
        QThread::msleep(15);
        if (heartbeat.hasExpired(m_heartbeat)) {
            if (!Storage::run(m_shell, {"image-selector", "ping"}, &output, error, 5000) || output.trimmed() != "ok") {
                if (error->isEmpty()) *error = "The Omarchy carousel disconnected while waiting for a selection.";
                return {};
            }
            heartbeat.restart();
        }
    }
    if (!QFileInfo::exists(selection) || QFileInfo(selection).size() == 0) return {};
    const QString chosen = QString::fromUtf8(Storage::read(selection, error, 32768)).trimmed();
    if (!error->isEmpty()) return {};
    for (const auto &value : batch.value("images").toArray()) {
        const auto image = value.toObject();
        if (image.value("alias").toString() == chosen) {
            const QString path = image.value("path").toString();
            if (QFileInfo(path).canonicalFilePath() != path) { *error = "Selected wallpaper disappeared or changed its target."; return {}; }
            return path;
        }
    }
    *error = "The carousel returned an image outside the prepared batch.";
    return {};
}
