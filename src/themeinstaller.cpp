#include "themeinstaller.h"
#include "storage.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QLockFile>
#include <QImageReader>
#include <QPainter>
#include <QSaveFile>
#include <QTemporaryDir>

ThemeInstaller::ThemeInstaller(QString config) : m_config(config.isEmpty() ? Storage::configRoot() : config) {}
QString ThemeInstaller::directory() const { return m_config + "/omarchy/themes/aether"; }

bool ThemeInstaller::stage(const Palette &palette, const QString &image, QString *error) {
    if (!palette.report().value("valid").toBool()) { *error = "Refusing to stage an invalid palette."; return false; }
    const QString original = QFileInfo(image).canonicalFilePath();
    if (original.isEmpty() || !QFileInfo(original).isFile()) { *error = "Wallpaper does not exist: " + image; return false; }
    const QString parent = m_config + "/omarchy/themes", destination = directory();
    QDir().mkpath(parent);
    QLockFile lock(parent + "/.omarchy-aether.lock");
    if (!lock.tryLock(5000)) { *error = "Aether theme files are busy."; return false; }
    if (QFileInfo(destination).isSymLink() || (QFileInfo::exists(destination) && !QFileInfo::exists(destination + "/.omarchy-aether.json"))) {
        *error = "An existing Aether theme is not managed by this project. Back it up before applying; it was not modified.";
        return false;
    }
    // Build the entire theme beside its destination; no partially written palette is ever staged.
    QTemporaryDir temporary(parent + "/.aether-stage-XXXXXX");
    if (!temporary.isValid()) { *error = "Cannot create the theme staging directory."; return false; }
    if (!Storage::write(temporary.filePath("colors.toml"), palette.toml(), error)
        || !Storage::writeJson(temporary.filePath(".omarchy-aether.json"), {{"version", 1}, {"wallpaper", original}}, error)) return false;
    QDir().mkpath(temporary.filePath("backgrounds"));
    const QString target = temporary.filePath("backgrounds/current." + QFileInfo(original).suffix().toLower());
    const QString activeTheme = QFileInfo(Storage::stateRoot() + "/theme").canonicalFilePath();
    const QString generatedTheme = QFileInfo(destination).canonicalFilePath();
    const bool transient = (!activeTheme.isEmpty() && original.startsWith(activeTheme + "/"))
        || (!generatedTheme.isEmpty() && original.startsWith(generatedTheme + "/"));
    // Omarchy replaces current/theme during a switch. A link into that directory would destroy its own source.
    // Stable library originals are linked; an already staged/current theme image is copied before replacement.
    if (!(transient ? QFile::copy(original, target) : QFile::link(original, target))) {
        *error = "Cannot preserve the original wallpaper in the theme.";
        return false;
    }
    // The native Theme menu discovers preview.png without any custom menu or launcher.
    QImageReader reader(original);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (size.isValid()) reader.setScaledSize(size.scaled(1536, 864, Qt::KeepAspectRatio));
    const QImage decoded = reader.read();
    if (decoded.isNull()) { *error = "Cannot create the native theme preview: " + reader.errorString(); return false; }
    QImage preview(1536, 864, QImage::Format_RGB32);
    preview.fill(QColor(palette.colors.value("background")));
    QPainter painter(&preview);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QRectF crop(decoded.rect());
    const double ratio = 1536.0 / 864;
    if (crop.width() / crop.height() > ratio) {
        const double width = crop.height() * ratio;
        crop.setLeft((crop.width() - width) / 2);
        crop.setWidth(width);
    } else {
        const double height = crop.width() / ratio;
        crop.setTop((crop.height() - height) / 2);
        crop.setHeight(height);
    }
    painter.drawImage(QRectF(0, 0, 1536, 864), decoded, crop);
    const auto names = Palette::ansiNames();
    for (int i = 0; i < names.size(); ++i)
        painter.fillRect((i % 8) * 192, 768 + (i / 8) * 48, 192, 48, QColor(palette.colors.value(names[i])));
    painter.end();
    QSaveFile previewFile(temporary.filePath("preview.png"));
    if (!previewFile.open(QIODevice::WriteOnly) || !preview.save(&previewFile, "PNG") || !previewFile.commit()) {
        *error = "Cannot save the native theme preview.";
        return false;
    }
    const QString backup = temporary.path() + "-previous";
    const bool existed = QFileInfo::exists(destination);
    if (existed && !QDir().rename(destination, backup)) { *error = "Cannot preserve the existing generated theme."; return false; }
    if (!QDir().rename(temporary.path(), destination)) {
        if (existed) QDir().rename(backup, destination);
        *error = "Cannot publish the generated theme.";
        return false;
    }
    temporary.setAutoRemove(false);
    if (existed && !QDir(backup).removeRecursively()) {
        *error = "Theme staged, but its previous generated copy could not be removed: " + backup;
        return false;
    }
    return true;
}

bool ThemeInstaller::apply(const Palette &palette, const QString &image, QString *error) {
    if (m_config != Storage::configRoot()) { *error = "Applying is only supported with the actual user configuration directory."; return false; }
    if (!stage(palette, image, error)) return false;
    QByteArray result;
    if (!Storage::run("omarchy", {"theme", "set", "aether"}, &result, error, 120000)) return false;
    if (!Storage::run("hyprctl", {"configerrors"}, &result, error, 5000)) return false;
    if (!result.trimmed().isEmpty() && result.trimmed() != "ok") { *error = "Hyprland reported configuration errors: " + QString::fromUtf8(result); return false; }
    return true;
}
