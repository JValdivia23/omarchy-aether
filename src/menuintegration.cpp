#include "menuintegration.h"
#include "storage.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLockFile>

QString MenuIntegration::masked(const QString &text, bool trailingCommas, QString *error) {
    QString result = text;
    bool quoted = false, escaped = false;
    for (int i = 0; i < result.size(); ++i) {
        const QChar ch = result[i];
        if (quoted) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') quoted = false;
            continue;
        }
        if (ch == '"') { quoted = true; continue; }
        if (ch == '/' && i + 1 < result.size() && result[i + 1] == '/') {
            while (i < result.size() && result[i] != '\n') result[i++] = ' ';
            --i;
        } else if (ch == '/' && i + 1 < result.size() && result[i + 1] == '*') {
            const auto end = result.indexOf("*/", i + 2);
            if (end < 0) { *error = "Unterminated comment in the Omarchy menu."; return {}; }
            while (i < end + 2) { if (result[i] != '\n') result[i] = ' '; ++i; }
            --i;
        } else if (trailingCommas && ch == ',') {
            int next = i + 1;
            while (next < result.size() && result[next].isSpace()) ++next;
            if (next < result.size() && (result[next] == '}' || result[next] == ']')) result[i] = ' ';
        }
    }
    return result;
}

QJsonObject MenuIntegration::parse(const QString &text, QString *error) {
    const QString comments = masked(text, false, error);
    if (!error->isEmpty()) return {};
    const QString clean = masked(comments, true, error);
    if (!error->isEmpty()) return {};
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(clean.toUtf8(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()) {
        *error = "The Omarchy menu is not a valid JSONC object: " + parse.errorString();
        return {};
    }
    return document.object();
}

QJsonObject MenuIntegration::install(const QString &menuPath, const QByteArray &fragmentData, QString *error, const QJsonObject &nativeRow) {
    const auto fragment = QJsonDocument::fromJson(fragmentData).object();
    const QJsonObject expected{{"appearance.background", QJsonObject{{"action", "omarchy-aether background"}}}};
    if (fragment != expected) { *error = "Integration accepts only the Aether Background menu action."; return {}; }
    // Omarchy normalizes missing row fields before merging, which would erase the stock label/icon/aliases.
    // Carry the installed native metadata forward instead of producing an action-only normalized row.
    QJsonObject rowData = nativeRow.isEmpty() ? QJsonObject{{"icon", QString::fromUtf8("")}, {"label", "Background"},
                                                           {"aliases", QJsonArray{"background", "wallpaper"}}} : nativeRow;
    rowData.insert("action", "omarchy-aether background");
    const QJsonObject extension{{"appearance.background", rowData}};
    if (QFileInfo(menuPath).isSymLink()) {
        *error = "The menu is managed through a symlink. Merge the Aether action into its backing dotfile explicitly; it was not modified.";
        return {};
    }
    if (!QDir().mkpath(QFileInfo(menuPath).absolutePath())) { *error = "Cannot create the menu directory."; return {}; }
    QLockFile lock(menuPath + ".aether.lock");
    if (!lock.tryLock(5000)) { *error = "The Omarchy menu integration is busy."; return {}; }
    const bool existed = QFileInfo::exists(menuPath);
    QString text = existed ? QString::fromUtf8(Storage::read(menuPath, error, 1024 * 1024)) : "{\n}\n";
    if (!error->isEmpty()) return {};
    const auto original = parse(text, error);
    if (!error->isEmpty()) return {};
    if (original.contains("appearance.background")) {
        if (original.value("appearance.background").toObject().value("action").toString() == "omarchy-aether background")
            return {{"valid", true}, {"changed", false}, {"menu", menuPath}};
        *error = "An existing Background menu customization was preserved. Merge the Aether action manually before retrying.";
        return {};
    }
    QString comments = masked(text, false, error);
    int closing = comments.lastIndexOf('}');
    if (!original.isEmpty()) {
        int last = closing - 1;
        while (last >= 0 && comments[last].isSpace()) --last;
        if (last >= 0 && comments[last] != ',') { text.insert(last + 1, ','); ++closing; }
    }
    QString row = QString::fromUtf8(QJsonDocument(extension).toJson(QJsonDocument::Indented));
    row = row.mid(2, row.size() - 4);
    if (closing > 0 && text[closing - 1] != '\n') row.prepend('\n');
    row.append('\n');
    text.insert(closing, row);
    const auto merged = parse(text, error);
    QJsonObject expectedMenu = original;
    expectedMenu.insert("appearance.background", rowData);
    if (!error->isEmpty() || merged != expectedMenu) {
        if (error->isEmpty()) *error = "The menu merge did not preserve the original configuration.";
        return {};
    }
    QString backup;
    if (existed) {
        backup = menuPath + ".before-aether-" + QString::number(QDateTime::currentMSecsSinceEpoch());
        if (!QFile::copy(menuPath, backup)) { *error = "Cannot back up the existing Omarchy menu."; return {}; }
    }
    if (!Storage::write(menuPath, text.toUtf8(), error)) return {};
    return {{"valid", true}, {"changed", true}, {"menu", menuPath}, {"backup", backup}};
}
