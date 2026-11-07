// Live language following and plural rules derive from the Omarchy app templates (MIT, David Heinemeier Hansson).
#include "translator.h"
#include "storage.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

Translator::Translator(QString file, QString catalogs, QObject *parent)
    : QTranslator(parent), m_file(file.isEmpty() ? Storage::configRoot() + "/omarchy/language" : file), m_catalogs(catalogs) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Translator::reload);
    reload();
}
QString Translator::pluralCategory(const QString &language, long long count) {
    const auto n = qAbs(count), n10 = n % 10, n100 = n % 100;
    const bool millions = n != 0 && n % 1000000 == 0, teens = n100 >= 12 && n100 <= 14;
    auto in = [&](std::initializer_list<const char *> codes) {
        for (const auto *code : codes) if (language == QLatin1String(code)) return true;
        return false;
    };
    if (in({"ja", "ko", "zh-CN", "zh-TW", "th", "vi", "id", "ms"})) return "other";
    if (in({"hi", "bn", "fa", "si"})) return n <= 1 ? "one" : "other";
    if (in({"fr", "pt-BR"})) return n <= 1 ? "one" : millions ? "many" : "other";
    if (in({"ca", "es-MX", "it", "pt"})) return n == 1 ? "one" : millions ? "many" : "other";
    if (language == "fil") return n10 == 4 || n10 == 6 || n10 == 9 ? "other" : "one";
    if (language == "is") return n10 == 1 && n100 != 11 ? "one" : "other";
    if (in({"cs", "sk"})) return n == 1 ? "one" : n >= 2 && n <= 4 ? "few" : "other";
    if (language == "pl") return n == 1 ? "one" : n10 >= 2 && n10 <= 4 && !teens ? "few" : "many";
    if (in({"ru", "uk", "hr", "sr"})) return n10 == 1 && n100 != 11 ? "one" : n10 >= 2 && n10 <= 4 && !teens ? "few" : language == "ru" || language == "uk" ? "many" : "other";
    if (language == "sl") return n100 == 1 ? "one" : n100 == 2 ? "two" : n100 >= 3 && n100 <= 4 ? "few" : "other";
    if (language == "lt") return n100 >= 11 && n100 <= 19 ? "other" : n10 == 1 ? "one" : n10 >= 2 && n10 <= 9 ? "few" : "other";
    if (language == "lv") return n10 == 0 || (n100 >= 11 && n100 <= 19) ? "zero" : n10 == 1 ? "one" : "other";
    if (language == "ro") return n == 1 ? "one" : n == 0 || (n100 >= 1 && n100 <= 19) ? "few" : "other";
    if (language == "ar") return n <= 2 ? n == 0 ? "zero" : n == 1 ? "one" : "two" : n100 >= 3 && n100 <= 10 ? "few" : n100 >= 11 ? "many" : "other";
    if (language == "he") return n == 1 ? "one" : n == 2 ? "two" : "other";
    if (language == "ga") return n == 1 ? "one" : n == 2 ? "two" : n <= 6 && n >= 3 ? "few" : n <= 10 && n >= 7 ? "many" : "other";
    return n == 1 ? "one" : "other";
}
QString Translator::translate(const char *, const char *text, const char *, int n) const {
    const auto value = m_strings.value(QString::fromUtf8(text));
    if (value.isString()) return value.toString();
    if (value.isObject()) {
        const auto forms = value.toObject();
        const QString result = forms.value(n < 0 ? "other" : pluralCategory(m_language, n)).toString();
        return result.isEmpty() ? forms.value("other").toString() : result;
    }
    return {};
}
void Translator::reload() {
    const auto old = m_watcher.files() + m_watcher.directories();
    if (!old.isEmpty()) m_watcher.removePaths(old);
    QString parent = QFileInfo(m_file).absolutePath();
    while (!QFileInfo::exists(parent) && parent != "/") parent = QFileInfo(parent).absolutePath();
    m_watcher.addPath(parent);
    if (QFileInfo::exists(m_file)) m_watcher.addPath(m_file);
    QFile file(m_file);
    QString language = file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readLine()).trimmed() : "en";
    if (!QRegularExpression("^[a-z]{2,3}(-[A-Za-z]{2})?$").match(language).hasMatch()) language = "en";
    if (language == m_language) return;
    m_strings.clear();
    QStringList codes{"en", language.section('-', 0, 0), language};
    codes.removeDuplicates();
    for (const auto &code : codes) {
        QFile catalog(m_catalogs + "/" + code + ".json");
        if (!catalog.open(QIODevice::ReadOnly)) continue;
        const auto object = QJsonDocument::fromJson(catalog.readAll()).object();
        for (auto it = object.begin(); it != object.end(); ++it)
            if (it.value().isObject() || (it.value().isString() && !it.value().toString().isEmpty())) m_strings.insert(it.key(), it.value());
    }
    m_language = language;
    QFile registry(qEnvironmentVariable("OMARCHY_PATH", "/usr/share/omarchy") + "/default/i18n/locales.json");
    QString localeName;
    if (registry.open(QIODevice::ReadOnly)) localeName = QJsonDocument::fromJson(registry.readAll()).object().value(language).toObject().value("formatLocale").toString();
    const QLocale locale(localeName.isEmpty() ? language : localeName);
    QLocale::setDefault(locale);
    QGuiApplication::setLayoutDirection(locale.textDirection());
    emit changed();
}
