#include "catalog.h"
#include "storage.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QLockFile>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <sys/resource.h>

namespace {
QString hash(const QByteArray &bytes) { return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex(); }
QStringList strings(const QJsonArray &array) {
    QStringList result;
    for (const auto &value : array) if (value.isString()) result << value.toString();
    return result;
}
void shuffle(QStringList &values) {
    for (int i = values.size() - 1; i > 0; --i) values.swapItemsAt(i, QRandomGenerator::global()->bounded(i + 1));
}
}

Catalog::Catalog(QString cache) : m_cache(cache.isEmpty() ? Storage::cacheRoot() : QDir(cache).absolutePath()) {}

QJsonObject Catalog::entry(const QString &path, const QString &cache) {
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    const QStringList supported{"jpg", "jpeg", "png", "webp", "bmp", "gif"};
    if (!info.isFile() || canonical.isEmpty() || !supported.contains(info.suffix().toLower())) return {};
    const QString id = hash(canonical.toUtf8());
    const QString signature = QString::number(info.size()) + ":" + QString::number(info.lastModified().toMSecsSinceEpoch());
    QString label = info.fileName();
    label.replace(QRegularExpression("[^a-zA-Z0-9._-]"), "_");
    // Omarchy deduplicates rows by basename, so stable unique aliases preserve equal filenames in different categories.
    const QString alias = cache + "/sources/" + id.left(20) + "-" + label;
    return {{"id", id}, {"path", canonical}, {"signature", signature}, {"alias", alias},
            {"thumbnail", cache + "/thumbnails/" + hash((canonical + "\n" + signature + "\n1536x864-v1").toUtf8()) + ".jpg"}};
}

bool Catalog::index(const QStringList &directories, QString *error) {
    if (directories.isEmpty()) { *error = "Provide at least one wallpaper directory."; return false; }
    QDir().mkpath(m_cache);
    QLockFile lock(m_cache + "/catalog.lock");
    if (!lock.tryLock(5000)) { *error = "The wallpaper catalog is busy."; return false; }
    QJsonArray roots, images;
    QSet<QString> seen;
    const QStringList extensions{"jpg", "jpeg", "png", "webp", "bmp", "gif"};
    for (const auto &directory : directories) {
        const QFileInfo root(directory);
        if (!root.isDir()) { *error = "Wallpaper directory does not exist: " + directory; return false; }
        roots.append(root.canonicalFilePath());
        QDirIterator iterator(root.canonicalFilePath(), QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString path = iterator.next();
            if (!extensions.contains(QFileInfo(path).suffix().toLower())) continue;
            const auto image = entry(path, m_cache);
            const QString id = image.value("id").toString();
            if (id.isEmpty() || seen.contains(id)) continue;
            if (seen.size() >= 50000) { *error = "The wallpaper catalog limit is 50,000 images."; return false; }
            seen.insert(id);
            images.append(image);
        }
    }
    if (images.isEmpty()) { *error = "No supported wallpapers were found."; return false; }
    return Storage::writeJson(m_cache + "/catalog.json", {{"version", 1}, {"roots", roots}, {"images", images}}, error);
}

QJsonObject Catalog::load(QString *error) const {
    const auto object = Storage::readJson(m_cache + "/catalog.json", error);
    if (!error->isEmpty()) return {};
    if (object.value("version").toInt() != 1 || !object.value("images").isArray()) {
        *error = "Unsupported wallpaper catalog. Re-run the index command.";
        return {};
    }
    return object;
}

bool Catalog::thumbnail(const QJsonObject &image, QString *error) const {
    const auto original = entry(image.value("path").toString(), m_cache);
    if (original.isEmpty() || original.value("signature") != image.value("signature")) {
        *error = "Wallpaper changed or disappeared; refresh the index: " + image.value("path").toString();
        return false;
    }
    const QString alias = original.value("alias").toString();
    QDir().mkpath(m_cache + "/sources");
    if (QFileInfo(alias).isSymLink()) {
        if (QFileInfo(alias).canonicalFilePath() != original.value("path").toString()) {
            *error = "Refusing to replace an unexpected wallpaper alias: " + alias;
            return false;
        }
    } else if (QFileInfo::exists(alias) || !QFile::link(original.value("path").toString(), alias)) {
        *error = "Cannot create a stable wallpaper alias: " + alias;
        return false;
    }
    const QString target = original.value("thumbnail").toString();
    if (QFileInfo(target).isFile()) return true;
    QDir().mkpath(m_cache + "/thumbnails");
    QLockFile lock(target + ".lock");
    if (!lock.tryLock(30000)) { *error = "Thumbnail is busy: " + target; return false; }
    if (QFileInfo(target).isFile()) return true;
    QByteArray result;
    const QString temporary = target + "." + QString::number(QCoreApplication::applicationPid()) + ".jpg";
    if (!Storage::run("env", {"VIPS_CONCURRENCY=1", "vipsthumbnail", original.value("path").toString(),
                              "--size", "1536x864", "--smartcrop=centre", "--path", temporary + "[Q=82,strip]"},
                      &result, error, 30000)) {
        QFile::remove(temporary);
        return false;
    }
    if (!QFile::rename(temporary, target)) { QFile::remove(temporary); *error = "Cannot publish thumbnail: " + target; return false; }
    return true;
}

bool Catalog::warm(int limit, QString *error) {
    const auto catalog = load(error);
    if (!error->isEmpty()) return false;
    setpriority(PRIO_PROCESS, 0, 10);
    int completed = 0;
    for (const auto &value : catalog.value("images").toArray()) {
        if (limit > 0 && completed >= limit) break;
        if (!thumbnail(value.toObject(), error)) return false;
        ++completed;
    }
    return true;
}

QJsonObject Catalog::prepare(int count, const QString &current, QString *error) {
    if (count < 1 || count > 200) { *error = "Batch size must be between 1 and 200."; return {}; }
    const auto catalog = load(error);
    if (!error->isEmpty()) return {};
    QLockFile lock(m_cache + "/prepare.lock");
    if (!lock.tryLock(5000)) { *error = "Another batch is being prepared."; return {}; }
    setpriority(PRIO_PROCESS, 0, 10);
    QMap<QString, QJsonObject> entries;
    QStringList ids;
    QString pinned;
    const QString canonicalCurrent = QFileInfo(current).canonicalFilePath();
    for (const auto &value : catalog.value("images").toArray()) {
        const auto object = value.toObject();
        const QString id = object.value("id").toString();
        entries.insert(id, object);
        if (object.value("path").toString() == canonicalCurrent) pinned = id;
        else ids << id;
    }
    const QString deckPath = m_cache + "/deck.json";
    QStringList deck;
    if (QFileInfo::exists(deckPath)) {
        const auto state = Storage::readJson(deckPath, error);
        if (!error->isEmpty()) return {};
        for (const auto &id : strings(state.value("remaining").toArray()))
            if (entries.contains(id) && id != pinned && !deck.contains(id)) deck << id;
    }
    QJsonArray batch;
    if (!canonicalCurrent.isEmpty()) {
        const auto image = entry(canonicalCurrent, m_cache);
        if (!image.isEmpty()) {
            if (!thumbnail(image, error)) return {};
            batch.append(image);
        }
    }
    QSet<QString> chosen;
    const int needed = std::min(count, int(ids.size()));
    while (chosen.size() < needed) {
        if (deck.isEmpty()) {
            deck = ids;
            shuffle(deck);
            for (const auto &id : chosen) deck.removeAll(id);
        }
        if (deck.isEmpty()) break;
        const QString id = deck.takeFirst();
        if (chosen.contains(id)) continue;
        const auto image = entries.value(id);
        if (!thumbnail(image, error)) return {};
        batch.append(image);
        chosen.insert(id);
    }
    if (batch.isEmpty()) { *error = "No wallpapers are ready."; return {}; }
    const QJsonObject ready{{"version", 1}, {"batch_size", count}, {"images", batch},
                            {"pinned", canonicalCurrent}, {"pinned_signature", entry(canonicalCurrent, m_cache).value("signature")}};
    if (!Storage::writeJson(deckPath, {{"remaining", QJsonArray::fromStringList(deck)}}, error)
        || !Storage::writeJson(m_cache + "/ready.json", ready, error)) return {};
    return ready;
}

QJsonObject Catalog::ready(QString *error) const {
    const auto object = Storage::readJson(m_cache + "/ready.json", error);
    if (!error->isEmpty()) return {};
    if (object.value("version").toInt() != 1 || object.value("images").toArray().isEmpty()) {
        *error = "No prepared wallpaper batch. Run the prepare command first.";
        return {};
    }
    return object;
}
