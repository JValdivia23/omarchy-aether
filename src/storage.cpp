#include "storage.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>

QByteArray Storage::read(const QString &path, QString *error, qint64 maximum) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { *error = f.errorString() + ": " + path; return {}; }
    if (f.size() > maximum) { *error = "File exceeds the size limit: " + path; return {}; }
    return f.readAll();
}
bool Storage::write(const QString &path, const QByteArray &data, QString *error) {
    if (QFileInfo(path).isSymLink()) { *error = "Refusing to replace a symlink: " + path; return false; }
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) { *error = "Cannot create parent directory: " + path; return false; }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        *error = file.errorString() + ": " + path;
        return false;
    }
    return true;
}
QJsonObject Storage::readJson(const QString &path, QString *error) {
    const auto data = read(path, error);
    if (!error->isEmpty()) return {};
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(data, &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()) { *error = "Invalid JSON: " + path; return {}; }
    return document.object();
}
bool Storage::writeJson(const QString &path, const QJsonObject &data, QString *error) {
    return write(path, QJsonDocument(data).toJson(QJsonDocument::Indented), error);
}
bool Storage::run(const QString &program, const QStringList &arguments, QByteArray *output, QString *error, int timeout) {
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForStarted(5000)) { *error = "Cannot start " + program + ": " + process.errorString(); return false; }
    if (!process.waitForFinished(timeout)) {
        process.kill();
        process.waitForFinished(1000);
        *error = "Timed out running " + program;
        return false;
    }
    *output = process.readAllStandardOutput();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        *error = program + ": " + QString::fromUtf8(process.readAllStandardError()).trimmed();
        if (error->endsWith(": ")) *error += "command failed";
        return false;
    }
    return true;
}
QString Storage::cacheRoot() { return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/omarchy-aether"; }
QString Storage::configRoot() { return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation); }
QString Storage::stateRoot() { return qEnvironmentVariable("XDG_STATE_HOME", QDir::homePath() + "/.local/state") + "/omarchy/current"; }
