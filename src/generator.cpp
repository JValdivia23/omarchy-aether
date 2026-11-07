#include "generator.h"
#include "storage.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>

Generator::Generator(QString cache, QString executable)
    : m_cache(cache.isEmpty() ? Storage::cacheRoot() : cache), m_executable(executable) {}

bool Generator::extract(const QJsonObject &json, QColor *source, QColor *surface, QString *error) {
    auto value = [&](const QString &key) {
        const auto token = json.value("colors").toObject().value(key);
        if (token.isString()) return token.toString();
        const auto object = token.toObject();
        const auto dark = object.value("dark");
        if (dark.isString()) return dark.toString();
        return dark.toObject().value("color").toString();
    };
    const QString seed = value("source_color"), background = value("surface");
    static const QRegularExpression hex("^#[0-9a-fA-F]{6}$");
    if (!hex.match(seed).hasMatch() || !hex.match(background).hasMatch()) {
        *error = "Matugen returned an unsupported or incomplete palette. Expected source_color and surface dark hex colors.";
        return false;
    }
    *source = QColor(seed);
    *surface = QColor(background);
    return true;
}

Palette Generator::generate(const QString &image, QString *error, int sourceIndex) {
    const QFileInfo info(image);
    if (!info.isFile() || info.canonicalFilePath().isEmpty()) { *error = "Wallpaper does not exist: " + image; return {}; }
    if (sourceIndex < 0 || sourceIndex > 3) { *error = "Source color index must be between 0 and 3."; return {}; }
    QByteArray version;
    if (!Storage::run(m_executable, {"--version"}, &version, error)) return {};
    const QByteArray signature = info.canonicalFilePath().toUtf8() + '\0' + QByteArray::number(info.size()) + ':'
        + QByteArray::number(info.lastModified().toMSecsSinceEpoch()) + ':' + version.trimmed()
        + ":aether-policy-v1:" + QByteArray::number(sourceIndex);
    const QString cacheFile = m_cache + "/palettes/" + QCryptographicHash::hash(signature, QCryptographicHash::Sha256).toHex() + ".json";
    QJsonObject data;
    if (QFileInfo::exists(cacheFile)) {
        data = Storage::readJson(cacheFile, error);
        if (!error->isEmpty()) return {};
    } else {
        QByteArray output;
        // Dry-run is essential: Matugen may have user templates or wallpaper hooks configured.
        if (!Storage::run(m_executable, {"image", info.canonicalFilePath(), "--dry-run", "--json", "hex",
                                        "--source-color-index", QString::number(sourceIndex), "--type", "scheme-tonal-spot", "--mode", "dark"},
                          &output, error, 60000)) return {};
        QJsonParseError parse;
        const auto document = QJsonDocument::fromJson(output, &parse);
        if (parse.error != QJsonParseError::NoError || !document.isObject()) { *error = "Matugen did not return valid JSON."; return {}; }
        data = document.object();
    }
    QColor seed, surface;
    if (!extract(data, &seed, &surface, error)) return {};
    const Palette palette = Palette::create(seed, surface);
    if (!palette.report().value("valid").toBool()) { *error = "Generated palette failed contrast or color-diversity validation."; return {}; }
    if (!Storage::writeJson(cacheFile, data, error)) return {};
    return palette;
}
