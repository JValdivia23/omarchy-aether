#include "cli.h"
#include "catalog.h"
#include "generator.h"
#include "huerotation.h"
#include "menuintegration.h"
#include "picker.h"
#include "storage.h"
#include "themeinstaller.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>
#include <cmath>

namespace {
void printJson(const QJsonObject &value) { QTextStream(stdout) << QJsonDocument(value).toJson(QJsonDocument::Indented); }
}

int runCli() {
    QCommandLineParser parser;
    parser.setApplicationDescription("Wallpaper-adaptive Omarchy palettes with verified contrast, distinct ANSI colors, and a fast native carousel.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("command", "open | generate | check | index | prepare | warm | pick | background | integrate | apply | skill install");
    parser.addPositionalArgument("paths", "Palette file, wallpaper directories, or 'install' for the skill command.", "[paths…]");
    parser.addOptions({{{"j", "json"}, "Output machine-readable JSON."},
                       {"image", "Wallpaper image.", "path"}, {"seed", "Generate from an explicit hex color instead of an image.", "hex"},
                       {{"o", "output"}, "Export colors.toml atomically to this file.", "path"},
                       {"cache-dir", "Use an isolated cache directory.", "path", Storage::cacheRoot()},
                       {"config-dir", "Configuration root for staging or explicit menu integration.", "path", Storage::configRoot()},
                       {"fragment", "Native menu integration fragment (optional).", "path"},
                       {"count", "Number of random candidates, excluding the pinned wallpaper (1–200).", "number", "35"},
                       {"source-index", "Matugen source candidate index (0–3).", "number", "0"},
                       {"hue-rotation", "Experimental hue rotation, 0–1; generate only, never applied.", "strength"},
                       {"preload", "Preload prepared rows into the running Omarchy shell."},
                       {"apply", "Apply the wallpaper chosen in the native picker."},
                       {"yes", "Confirm changing the active Omarchy theme."},
                       {"stage-only", "Write the generated theme without activating it."}});
    parser.process(*QCoreApplication::instance());
    const QStringList positional = parser.positionalArguments();
    const QString command = positional.value(0);
    const QString cache = QDir(parser.value("cache-dir")).absolutePath();
    QString error;
    auto fail = [&](const QString &message) {
        QTextStream(stderr) << message << '\n';
        if (parser.isSet("json")) printJson({{"valid", false}, {"error", message}});
        return 1;
    };
    auto finish = [&](const QJsonObject &object, const QString &message) {
        if (parser.isSet("json")) printJson(object); else QTextStream(stdout) << message << '\n';
        return 0;
    };
    bool countOk = false, indexOk = false;
    const int count = parser.value("count").toInt(&countOk), sourceIndex = parser.value("source-index").toInt(&indexOk);
    if (!countOk || count < 1 || count > 200 || !indexOk || sourceIndex < 0 || sourceIndex > 3) return fail("Invalid batch size or source color index.");
    double rotationStrength = 0;
    if (parser.isSet("hue-rotation")) {
        if (command != "generate") return fail("Experimental hue rotation is preview/export only; use generate, not apply or the native picker.");
        bool strengthOk = false;
        rotationStrength = parser.value("hue-rotation").toDouble(&strengthOk);
        if (!strengthOk || !std::isfinite(rotationStrength) || rotationStrength < 0 || rotationStrength > 1)
            return fail("Hue rotation strength must be between 0 and 1.");
    }
    auto generate = [&]() {
        if (parser.isSet("seed")) {
            if (!QRegularExpression("^#[0-9a-fA-F]{6}$").match(parser.value("seed")).hasMatch()) { error = "Seed must be a six-digit hex color."; return Palette(); }
            return Palette::create(QColor(parser.value("seed")));
        }
        return Generator(cache).generate(parser.value("image"), &error, sourceIndex);
    };

    if (command == "integrate") {
        if (!parser.isSet("yes")) return fail("Menu integration changes the user menu. Re-run with --yes.");
        const QString root = QDir(parser.value("config-dir")).absolutePath();
        const QString preference = root + "/omarchy-aether/menu.json";
        const QString source = parser.isSet("fragment") ? parser.value("fragment") : QFileInfo::exists(preference) ? preference : ":/native-menu.json";
        const auto fragment = Storage::read(source, &error, 32768);
        if (!error.isEmpty()) return fail(error);
        QJsonObject nativeRow;
        if (root == Storage::configRoot()) {
            const QString defaults = qEnvironmentVariable("OMARCHY_PATH", "/usr/share/omarchy") + "/default/omarchy/omarchy-menu.jsonc";
            const auto stock = MenuIntegration::parse(QString::fromUtf8(Storage::read(defaults, &error)), &error);
            if (!error.isEmpty()) return fail(error);
            if (!stock.contains("appearance.background")) return fail("The installed Omarchy Background menu ID is not supported; the user menu was not modified.");
            nativeRow = stock.value("appearance.background").toObject();
        }
        const auto result = MenuIntegration::install(root + "/omarchy/extensions/omarchy-menu.jsonc", fragment, &error, nativeRow);
        if (!error.isEmpty()) return fail(error);
        return finish(result, result.value("changed").toBool() ? "Native Background menu integrated. The active theme is unchanged." : "Native Background menu already integrated.");
    }

    if (command == "generate" || command == "apply") {
        if (command == "apply" && !parser.isSet("yes") && !parser.isSet("stage-only")) return fail("Applying changes the desktop. Re-run with --yes, or export with --stage-only.");
        auto palette = generate();
        if (!error.isEmpty()) return fail(error);
        QJsonObject experiment;
        if (parser.isSet("hue-rotation")) {
            const auto targets = parser.isSet("seed") ? QList<QColor>{QColor(parser.value("seed"))}
                                                     : HueRotation::sampleFile(parser.value("image"), &error);
            if (!error.isEmpty()) return fail(error);
            palette = HueRotation::rotate(palette, targets, rotationStrength, &error, &experiment);
            if (!error.isEmpty()) return fail(error);
        }
        auto report = palette.report();
        if (!report.value("valid").toBool()) return fail("Palette validation failed.");
        if (!experiment.isEmpty()) report["experiment"] = experiment;
        if (parser.isSet("output") && !Storage::write(parser.value("output"), palette.toml(), &error)) return fail(error);
        if (command == "apply") {
            const QString config = QDir(parser.value("config-dir")).absolutePath();
            ThemeInstaller installer(config);
            const bool ok = parser.isSet("stage-only") ? installer.stage(palette, parser.value("image"), &error)
                                                       : installer.apply(palette, parser.value("image"), &error);
            if (!ok) return fail(error);
            report["theme_directory"] = installer.directory();
            report["applied"] = !parser.isSet("stage-only");
        }
        if (parser.isSet("json")) printJson(report);
        else if (command == "generate" && !parser.isSet("output")) QTextStream(stdout) << palette.toml();
        else QTextStream(stdout) << (command == "generate" ? "Palette exported and validated." : parser.isSet("stage-only") ? "Theme staged; the active theme is unchanged." : "Aether applied.") << '\n';
        return 0;
    }
    if (command == "check") {
        const auto data = Storage::read(positional.value(1), &error);
        if (!error.isEmpty()) return fail(error);
        const auto palette = Palette::fromToml(data, &error);
        if (!error.isEmpty()) return fail(error);
        const auto report = palette.report();
        if (parser.isSet("json")) printJson(report);
        else {
            for (const auto &value : report.value("checks").toArray()) {
                const auto check = value.toObject();
                if (!check.value("passed").toBool()) QTextStream(stderr) << check.value("name").toString() << ": "
                    << check.value("value").toDouble() << " < " << check.value("minimum").toDouble() << '\n';
            }
            QTextStream(stdout) << (report.value("valid").toBool() ? "Palette passed." : "Palette failed.") << '\n';
        }
        if (!report.value("valid").toBool()) { QTextStream(stderr) << "Palette validation failed.\n"; return 1; }
        return 0;
    }
    Catalog catalog(cache);
    const QString current = QFileInfo(Storage::stateRoot() + "/background").canonicalFilePath();
    const bool nativeBackground = command == "background";
    QString backgroundTheme;
    auto currentTheme = [&]() {
        return QString::fromUtf8(Storage::read(Storage::stateRoot() + "/theme.name", &error, 4096)).trimmed();
    };
    if (nativeBackground) {
        backgroundTheme = currentTheme();
        if (!error.isEmpty()) return fail(error);
        if (backgroundTheme != "aether") {
            QByteArray output;
            if (!Storage::run("omarchy", {"theme", "bg-switcher"}, &output, &error, -1)) return fail(error);
            const QString selected = QString::fromUtf8(output).trimmed();
            if (selected.isEmpty()) return finish({{"valid", true}, {"cancelled", true}}, "Selection canceled.");
            if (currentTheme() != backgroundTheme) return fail("The active theme changed while the picker was open; nothing was applied.");
            if (!error.isEmpty()) return fail(error);
            if (!Storage::run("omarchy", {"theme", "bg", "set", selected}, &output, &error)) return fail(error);
            return finish({{"valid", true}, {"wallpaper", selected}, {"applied", true}}, "Background applied; static theme colors were preserved.");
        }
    }
    if (command == "index") {
        if (!catalog.index(positional.mid(1), &error)) return fail(error);
        const int total = catalog.load(&error).value("images").toArray().size();
        if (!error.isEmpty()) return fail(error);
        return finish({{"valid", true}, {"count", total}}, QString("Indexed %1 wallpapers. Run prepare to warm the first batch.").arg(total));
    }
    if (command == "warm" || command == "prepare") {
        if (command == "warm" && !catalog.warm(parser.isSet("count") ? count : 0, &error)) return fail(error);
        const auto batch = catalog.prepare(count, current, &error);
        if (!error.isEmpty()) return fail(error);
        if (parser.isSet("preload") && !Picker(cache).preload(batch, current, &error)) return fail(error);
        return finish({{"valid", true}, {"count", batch.value("images").toArray().size()}}, "Wallpaper batch cached and ready.");
    }
    if (command == "pick" || nativeBackground) {
        QDir().mkpath(cache);
        QLockFile lock(cache + "/picker.lock");
        if (!lock.tryLock(0)) return fail("The Aether picker is already open.");
        QJsonObject batch;
        if (QFileInfo::exists(cache + "/ready.json")) {
            batch = catalog.ready(&error);
            const auto pinned = Catalog::entry(current, cache);
            // Switching themes outside Aether can replace the current image at the same path.
            // Only that one file is checked on opening; the collection is never rescanned here.
            if (error.isEmpty() && (batch.value("pinned").toString() != current
                || batch.value("pinned_signature") != pinned.value("signature"))) batch = catalog.prepare(count, current, &error);
        }
        else {
            QTextStream(stderr) << "Preparing the first wallpaper batch. Future openings use prewarmed previews.\n";
            batch = catalog.prepare(count, current, &error);
        }
        if (!error.isEmpty()) return fail(error);
        const QString selected = Picker(cache).select(batch, current, &error);
        if (!error.isEmpty()) return fail(error);
        const bool applySelection = parser.isSet("apply") || nativeBackground;
        if (!selected.isEmpty() && nativeBackground && currentTheme() != "aether")
            return fail("The active theme changed while the picker was open; nothing was applied.");
        if (!error.isEmpty()) return fail(error);
        if (!selected.isEmpty() && applySelection) {
            const auto palette = Generator(cache).generate(selected, &error, sourceIndex);
            if (!error.isEmpty() || !ThemeInstaller().apply(palette, selected, &error)) return fail(error);
        }
        // The next batch is prepared only after dismissal, so browsing never rearranges the current choices.
        QProcess next;
        next.setProgram(QCoreApplication::applicationFilePath());
        next.setArguments({"prepare", "--cache-dir", cache, "--count", QString::number(count), "--preload", "--json"});
        next.setStandardOutputFile(cache + "/preparation.log", QIODevice::Append);
        next.setStandardErrorFile(cache + "/preparation.log", QIODevice::Append);
        if (!next.startDetached()) return fail("The selection finished, but the next batch could not be scheduled.");
        return finish({{"valid", true}, {"wallpaper", selected}, {"cancelled", selected.isEmpty()},
                       {"applied", !selected.isEmpty() && applySelection}}, selected);
    }
    if (command == "skill" && positional.value(1) == "install") {
        const QString target = QDir::homePath() + "/.agents/skills/omarchy-aether/SKILL.md";
        const auto contents = Storage::read(":/agent-skill.md", &error);
        if (!error.isEmpty()) return fail(error);
        if (QFileInfo::exists(target)) return fail("The Aether agent skill already exists; it was not overwritten.");
        if (!Storage::write(target, contents, &error)) return fail(error);
        return finish({{"valid", true}, {"path", target}}, "Aether agent skill installed.");
    }
    return fail("Unknown command. Run omarchy-aether --help.");
}
