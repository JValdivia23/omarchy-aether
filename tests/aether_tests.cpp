#include "backend.h"
#include "catalog.h"
#include "colorutils.h"
#include "generator.h"
#include "picker.h"
#include "storage.h"
#include "theme.h"
#include "themeinstaller.h"
#include "translator.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLinearGradient>
#include <QPainter>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class FakeFilePicker : public FilePicker {
public:
    int wallpapers = 0, collections = 0;
    void openWallpaper() override { ++wallpapers; }
    void openCollection() override { ++collections; }
};

class AetherTests : public QObject {
    Q_OBJECT
private:
    static void write(const QString &path, const QByteArray &data) {
        QString error;
        QVERIFY2(Storage::write(path, data, &error), qPrintable(error));
    }
    static void executable(const QString &path, const QByteArray &data) {
        write(path, data);
        QVERIFY(QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    }
    static void image(const QString &path, const QColor &color = QColor("#7aa2f7")) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QImage image(128, 72, QImage::Format_RGB32);
        image.fill(color);
        QVERIFY(image.save(path));
    }
    static QSet<QString> ids(const QJsonObject &batch, const QString &except = {}) {
        QSet<QString> result;
        for (const auto &value : batch.value("images").toArray())
            if (value.toObject().value("path").toString() != except) result.insert(value.toObject().value("id").toString());
        return result;
    }
    static QByteArray matugenJson(const QString &seed = "#ff66aa") {
        return QJsonDocument(QJsonObject{{"colors", QJsonObject{
            {"source_color", QJsonObject{{"dark", QJsonObject{{"color", seed}}}}},
            {"surface", QJsonObject{{"dark", QJsonObject{{"color", "#1a1b26"}}}}}}}}).toJson(QJsonDocument::Compact);
    }

private slots:
    void contrastReferenceValues() {
        QCOMPARE(Color::contrast(Qt::black, Qt::white), 21.0);
        QVERIFY(qAbs(Color::contrast(QColor("#a9b1d6"), QColor("#1a1b26")) - 8.10) < .01);
        QVERIFY(qAbs(Color::contrast(QColor("#c0caf5"), QColor("#292e42")) - 8.32) < .01);
    }
    void colorRoundTripsWithoutHueClipping() {
        for (const auto &hex : {"#7aa2f7", "#f7768e", "#9ece6a", "#449dab", "#ffffff", "#000000"}) {
            const QColor color(hex);
            QVERIFY(Color::distance(color, Color::fromLch(Color::toLch(color))) < .05);
        }
    }
    void everyHueProducesReadableDiverseColors() {
        for (int hue = 0; hue < 360; hue += 5) {
            const auto palette = Palette::create(QColor::fromHsv(hue, 200, 220));
            const auto report = palette.report();
            QVERIFY2(report.value("valid").toBool(), qPrintable(QString::number(hue) + ": " + QJsonDocument(report).toJson(QJsonDocument::Compact)));
        }
    }
    void monochromeAndExtremeSurfacesPass() {
        for (const auto &seed : {"#000000", "#ffffff", "#777777", "#ff0000", "#00ff00", "#0000ff", "#ff00ff"}) {
            for (const auto &surface : {"#000000", "#ffffff", "#777777", "#ff0000"}) {
                const auto report = Palette::create(QColor(seed), QColor(surface)).report();
                QVERIFY2(report.value("valid").toBool(), qPrintable(QString(seed) + "/" + surface + ": " + QJsonDocument(report).toJson(QJsonDocument::Compact)));
            }
        }
    }
    void paletteRoundTrip() {
        const auto palette = Palette::create(QColor("#d699b6"));
        QString error;
        const auto parsed = Palette::fromToml(palette.toml(), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(parsed.colors, palette.colors);
        QVERIFY(parsed.report().value("valid").toBool());
    }
    void invalidAndDuplicateTomlRejected() {
        QString error;
        Palette::fromToml("accent = \"#ffffff\"\naccent = \"#000000\"\n", &error);
        QVERIFY(!error.isEmpty());
        error.clear();
        Palette::fromToml("background = \"not-a-color\"\n", &error);
        QVERIFY(!error.isEmpty());
        QVERIFY(!Palette().report().value("valid").toBool());
    }
    void validatorDetectsCollapsedPalette() {
        auto palette = Palette::create(QColor("#7aa2f7"));
        for (const auto &name : Palette::chromaticNames()) palette.colors[name] = "#bb9af7";
        QVERIFY(!palette.report().value("valid").toBool());
    }
    void validatorDetectsFaintText() {
        auto palette = Palette::create(QColor("#7aa2f7"));
        palette.colors["muted"] = "#414868";
        QVERIFY(!palette.report().value("valid").toBool());
    }
    void matugenJsonIsValidated() {
        QString error;
        QColor source, surface;
        QVERIFY(Generator::extract(QJsonDocument::fromJson(matugenJson()).object(), &source, &surface, &error));
        QCOMPARE(source.name(), QString("#ff66aa"));
        QCOMPARE(surface.name(), QString("#1a1b26"));
        QVERIFY(!Generator::extract({}, &source, &surface, &error));
    }
    void generatorUsesDryRunAndPersistentCache() {
        QTemporaryDir temp;
        const QString wallpaper = temp.filePath("sample.png"), tool = temp.filePath("matugen"), args = temp.filePath("arguments");
        image(wallpaper);
        executable(tool, "#!/bin/sh\nif [ \"$1\" = --version ]; then echo 'matugen 4.2.0'; exit 0; fi\nprintf '%s\\n' \"$@\" > '" + args.toUtf8() + "'\nprintf '%s' '" + matugenJson() + "'\n");
        Generator generator(temp.filePath("cache"), tool);
        QString error;
        const auto first = generator.generate(wallpaper, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(first.report().value("valid").toBool());
        const auto arguments = Storage::read(args, &error);
        QVERIFY(arguments.contains("--dry-run"));
        QVERIFY(arguments.contains("--source-color-index\n0"));
        QVERIFY(QFile::remove(args));
        const auto second = generator.generate(wallpaper, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(first.colors, second.colors);
        QVERIFY(!QFileInfo::exists(args));
    }
    void generatorRejectsMalformedOutput() {
        QTemporaryDir temp;
        const QString wallpaper = temp.filePath("sample.png"), tool = temp.filePath("matugen");
        image(wallpaper);
        executable(tool, "#!/bin/sh\nif [ \"$1\" = --version ]; then echo v4; else echo '{\"colors\":{}}'; fi\n");
        QString error;
        Generator(temp.filePath("cache"), tool).generate(wallpaper, &error);
        QVERIFY(error.contains("incomplete"));
    }
    void writesNeverFollowSymlinks() {
        QTemporaryDir temp;
        const QString original = temp.filePath("original"), link = temp.filePath("link");
        write(original, "keep me");
        QVERIFY(QFile::link(original, link));
        QString error;
        QVERIFY(!Storage::write(link, "replace", &error));
        error.clear();
        QCOMPARE(Storage::read(original, &error), QByteArray("keep me"));
    }
    void catalogDeduplicatesRealPathsAndHandlesDuplicateNames() {
        QTemporaryDir temp;
        image(temp.filePath("walls/forest/same.png"));
        image(temp.filePath("walls/space/same.png"), Qt::red);
        QVERIFY(QFile::link(temp.filePath("walls/forest/same.png"), temp.filePath("walls/alias.png")));
        Catalog catalog(temp.filePath("cache"));
        QString error;
        QVERIFY2(catalog.index({temp.filePath("walls")}, &error), qPrintable(error));
        const auto entries = catalog.load(&error).value("images").toArray();
        QCOMPARE(entries.size(), 2);
        QVERIFY(QFileInfo(entries[0].toObject().value("alias").toString()).fileName() != QFileInfo(entries[1].toObject().value("alias").toString()).fileName());
    }
    void cachedThumbnailsSurviveReshuffling() {
        QTemporaryDir temp;
        const QString root = temp.filePath("walls");
        for (int i = 0; i < 9; ++i) image(root + "/image-" + QString::number(i) + ".png", QColor::fromHsv(i * 35, 200, 180));
        Catalog catalog(temp.filePath("cache"));
        QString error;
        QVERIFY2(catalog.index({root}, &error), qPrintable(error));
        const QString current = QFileInfo(root + "/image-0.png").canonicalFilePath();
        const auto first = catalog.prepare(3, current, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(first.value("images").toArray().size(), 4);
        const auto second = catalog.prepare(3, current, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        auto overlap = ids(first, current);
        overlap.intersect(ids(second, current));
        QVERIFY(overlap.isEmpty());
        const auto pinned = first.value("images").toArray().first().toObject();
        const QString thumbnail = pinned.value("thumbnail").toString();
        const auto timestamp = QFileInfo(thumbnail).lastModified();
        QVERIFY(QFileInfo(thumbnail).isFile());
        QVERIFY2(catalog.thumbnail(pinned, &error), qPrintable(error));
        QCOMPARE(QFileInfo(thumbnail).lastModified(), timestamp);
        QCOMPARE(QFileInfo(pinned.value("alias").toString()).canonicalFilePath(), current);
    }
    void changedImagesGetNewThumbnailKeys() {
        QTemporaryDir temp;
        const QString path = temp.filePath("sample.png");
        image(path);
        const auto before = Catalog::entry(path, temp.filePath("cache"));
        image(path, Qt::green);
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.setFileTime(QDateTime::currentDateTime().addSecs(2), QFileDevice::FileModificationTime));
        file.close();
        const auto after = Catalog::entry(path, temp.filePath("cache"));
        QCOMPARE(before.value("id"), after.value("id"));
        QCOMPARE(before.value("alias"), after.value("alias"));
        QVERIFY(before.value("thumbnail") != after.value("thumbnail"));
    }
    void nonImageCurrentBackgroundDoesNotBlockStaticCollections() {
        QTemporaryDir temp;
        const QString root = temp.filePath("walls"), current = temp.filePath("current.mp4");
        image(root + "/sample.png");
        write(current, "not a static image");
        Catalog catalog(temp.filePath("cache"));
        QString error;
        QVERIFY(catalog.index({root}, &error));
        const auto ready = catalog.prepare(35, current, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(ready.value("images").toArray().size(), 1);
        QCOMPARE(ready.value("pinned").toString(), QFileInfo(current).canonicalFilePath());
    }
    void originalFilesAreNeverModified() {
        QTemporaryDir temp;
        const QString wallpaper = temp.filePath("walls/sample.png");
        image(wallpaper);
        QString error;
        const auto original = Storage::read(wallpaper, &error);
        Catalog catalog(temp.filePath("cache"));
        QVERIFY(catalog.index({temp.filePath("walls")}, &error));
        catalog.prepare(35, wallpaper, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        ThemeInstaller installer(temp.filePath("config"));
        QVERIFY2(installer.stage(Palette::create(QColor("#ff55aa")), wallpaper, &error), qPrintable(error));
        QVERIFY2(installer.stage(Palette::create(QColor("#55aaff")), wallpaper, &error), qPrintable(error));
        QCOMPARE(Storage::read(wallpaper, &error), original);
        QVERIFY(!QFileInfo(installer.directory() + "/colors.toml").isSymLink());
    }
    void unmanagedThemesAreProtected() {
        QTemporaryDir temp;
        const QString wallpaper = temp.filePath("sample.png");
        image(wallpaper);
        ThemeInstaller installer(temp.filePath("config"));
        write(installer.directory() + "/colors.toml", "user work");
        QString error;
        QVERIFY(!installer.stage(Palette::create(Qt::blue), wallpaper, &error));
        QVERIFY(error.contains("not managed"));
        error.clear();
        QCOMPARE(Storage::read(installer.directory() + "/colors.toml", &error), QByteArray("user work"));
    }
    void invalidPalettesCannotBeStaged() {
        QTemporaryDir temp;
        const QString wallpaper = temp.filePath("sample.png");
        image(wallpaper);
        ThemeInstaller installer(temp.filePath("config"));
        QString error;
        QVERIFY(!installer.stage({}, wallpaper, &error));
        QVERIFY(!QFileInfo::exists(installer.directory()));
    }
    void stagingPreservesAnImageFromTheDirectoryBeingReplaced() {
        QTemporaryDir temp;
        const QString wallpaper = temp.filePath("sample.png");
        image(wallpaper);
        ThemeInstaller installer(temp.filePath("config"));
        QString error;
        const auto palette = Palette::create(QColor("#7aa2f7"));
        QVERIFY(installer.stage(palette, wallpaper, &error));
        const QString transient = installer.directory() + "/backgrounds/incoming.png";
        image(transient, Qt::red);
        const auto original = Storage::read(transient, &error);
        QVERIFY2(installer.stage(palette, transient, &error), qPrintable(error));
        const QString preserved = installer.directory() + "/backgrounds/current.png";
        QVERIFY(QFileInfo(preserved).isFile());
        QVERIFY(!QFileInfo(preserved).isSymLink());
        QCOMPARE(Storage::read(preserved, &error), original);
    }
    void pickerUsesNativeRowsAndValidatesSelection() {
        QTemporaryDir temp;
        const QString wallpaper = temp.filePath("sample.png"), shell = temp.filePath("shell");
        image(wallpaper);
        const auto entry = Catalog::entry(wallpaper, temp.filePath("cache"));
        const QJsonObject batch{{"images", QJsonArray{entry}}};
        const auto rows = Picker::rows(batch);
        QVERIFY(rows.contains(entry.value("alias").toString().toUtf8()));
        QVERIFY(rows.contains(entry.value("thumbnail").toString().toUtf8()));
        executable(shell, "#!/bin/sh\nif [ \"$2\" = open ]; then printf '%s' '" + entry.value("alias").toString().toUtf8() + "' > \"$6\"; touch \"$7\"; echo ok; fi\n");
        QString error;
        QCOMPARE(Picker(temp.filePath("cache"), shell).select(batch, wallpaper, &error), QFileInfo(wallpaper).canonicalFilePath());
        QVERIFY2(error.isEmpty(), qPrintable(error));
        executable(shell, "#!/bin/sh\nprintf '%s' /outside/batch > \"$6\"; touch \"$7\"; echo ok\n");
        QVERIFY(Picker(temp.filePath("cache"), shell).select(batch, wallpaper, &error).isEmpty());
        QVERIFY(error.contains("outside"));
    }
    void pickerCancellationChangesNothing() {
        QTemporaryDir temp;
        const QString shell = temp.filePath("shell");
        executable(shell, "#!/bin/sh\ntouch \"$7\"; echo ok\n");
        QString error;
        QVERIFY(Picker(temp.filePath("cache"), shell).select({{"images", QJsonArray{}}}, {}, &error).isEmpty());
        QVERIFY2(error.isEmpty(), qPrintable(error));
    }
    void pickerRejectsUnsupportedPreloadProtocol() {
        QTemporaryDir temp;
        const QString shell = temp.filePath("shell");
        executable(shell, "#!/bin/sh\necho unknown\n");
        QString error;
        QVERIFY(!Picker(temp.filePath("cache"), shell).preload({}, {}, &error));
        QVERIFY(error.contains("preload"));
    }
    void pickerDoesNotWaitForeverForADisconnectedShell() {
        QTemporaryDir temp;
        const QString shell = temp.filePath("shell");
        executable(shell, "#!/bin/sh\nif [ \"$2\" = open ]; then echo ok; else echo disconnected >&2; exit 1; fi\n");
        QString error;
        QVERIFY(Picker(temp.filePath("cache"), shell, 1).select({}, {}, &error).isEmpty());
        QVERIFY(error.contains("disconnected"));
    }
    void themeUsesFallbackAndRearmsAfterAtomicReplacement() {
        QTemporaryDir temp;
        const QString root = temp.filePath("current");
        Theme theme(root);
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
        write(root + "/theme/colors.toml", "accent = \"#112233\"\n");
        QTRY_COMPARE(theme.accent(), QString("#112233"));
        QVERIFY(QDir().rename(root + "/theme", root + "/previous"));
        write(root + "/theme/colors.toml", "accent = \"#ff6699\"\n");
        QTRY_COMPARE(theme.accent(), QString("#ff6699"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }
    void themeTracksEffectiveRounding() {
        QTemporaryDir temp;
        const QString command = temp.filePath("hyprctl"), response = temp.filePath("response");
        executable(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        write(response, "{\"int\":0}");
        Theme theme(temp.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }
    void translatorFollowsLanguageAndPluralRules() {
        QTemporaryDir temp;
        const QString path = temp.filePath("language");
        Translator translator(path);
        QCOMPARE(translator.language(), QString("en"));
        write(path, "da\n");
        QTRY_COMPARE(translator.language(), QString("da"));
        QCOMPARE(translator.translate("Main", "Choose wallpaper"), QString("Vælg baggrundsbillede"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ru", 22), QString("few"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }
    void backendUsesInjectedPortalAndPreviewDoesNotApply() {
        FakeFilePicker picker;
        Backend backend(&picker);
        backend.chooseWallpaper();
        backend.chooseCollection();
        QCOMPARE(picker.wallpapers, 1);
        QCOMPARE(picker.collections, 1);
        backend.setPreview(Palette::create(QColor("#7aa2f7")));
        QVERIFY(backend.ready());
        QCOMPARE(backend.colors().size(), 16);
        QVERIFY(!backend.busy());
    }
    void realQmlRendersAndTranslatesWithoutDesktopChanges() {
        QTemporaryDir temp;
        const QString wallpaper = temp.filePath("sample-landscape.png");
        QImage landscape(900, 520, QImage::Format_RGB32);
        QPainter painter(&landscape);
        QLinearGradient sky(0, 0, 0, 520);
        sky.setColorAt(0, QColor("#15233b")); sky.setColorAt(.65, QColor("#596b96")); sky.setColorAt(1, QColor("#d1a09a"));
        painter.fillRect(landscape.rect(), sky);
        painter.setBrush(QColor("#d5d9e8")); painter.setPen(Qt::NoPen); painter.drawEllipse(QPoint(680, 100), 32, 32);
        painter.setBrush(QColor("#394a62")); painter.drawPolygon(QPolygon{{0, 430}, {280, 160}, {520, 400}, {710, 210}, {900, 460}, {900, 520}, {0, 520}});
        painter.setBrush(QColor("#1e2b40")); painter.drawPolygon(QPolygon{{0, 490}, {180, 350}, {430, 490}, {700, 320}, {900, 450}, {900, 520}, {0, 520}});
        painter.end();
        QVERIFY(landscape.save(wallpaper));
        Theme theme(temp.filePath("current"));
        FakeFilePicker picker;
        Backend backend(&picker);
        backend.setImage(QUrl::fromLocalFile(wallpaper));
        backend.setPreview(Palette::create(QColor("#7aa2f7")));
        const QString language = temp.filePath("language");
        Translator translator(language);
        QCoreApplication::installTranslator(&translator);
        QQmlApplicationEngine engine;
        connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
        connect(&translator, &Translator::changed, &backend, &Backend::refreshLanguage);
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.rootContext()->setContextProperty("translator", &translator);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QTRY_VERIFY(window->isExposed());
        auto *button = window->findChild<QQuickItem *>("chooseWallpaper");
        QVERIFY(button);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, button->mapToScene(QPointF(button->width()/2, button->height()/2)).toPoint());
        QCOMPARE(picker.wallpapers, 1);
        auto *help = window->findChild<QObject *>("shortcutHelp");
        QVERIFY(help);
        QTest::keyClick(window, Qt::Key_Question);
        QTRY_VERIFY(help->property("visible").toBool());
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!help->property("visible").toBool());
        QDir().mkpath("screenshots");
        QTest::qWait(200);
        const auto screenshot = window->grabWindow();
        QVERIFY(!screenshot.isNull());
        QVERIFY(screenshot.save("screenshots/aether.png"));
        write(language, "da\n");
        QTRY_COMPARE(translator.language(), QString("da"));
        QTRY_COMPARE(button->property("text").toString(), QString("Vælg baggrundsbillede"));
        QTest::qWait(100);
        QVERIFY(window->grabWindow().save("screenshots/aether-da.png"));
        write(language, "ar\n");
        QTRY_COMPARE(translator.language(), QString("ar"));
        QCOMPARE(QGuiApplication::layoutDirection(), Qt::RightToLeft);
        QTest::qWait(100);
        QVERIFY(window->grabWindow().save("screenshots/aether-rtl.png"));
        QCOMPARE(warnings.count(), 0);
        QCoreApplication::removeTranslator(&translator);
        QGuiApplication::setLayoutDirection(Qt::LeftToRight);
    }
    void headlessCliUsesIsolatedOutput() {
        QTemporaryDir temp;
        const QString binary = QCoreApplication::applicationDirPath() + "/../omarchy-aether";
        const QString target = temp.filePath("colors.toml");
        QByteArray output;
        QString error;
        QVERIFY2(Storage::run(binary, {"generate", "--seed", "#ff66aa", "--output", target, "--json", "--cache-dir", temp.filePath("cache")}, &output, &error), qPrintable(error));
        QVERIFY(QJsonDocument::fromJson(output).object().value("valid").toBool());
        QVERIFY2(Storage::run(binary, {"check", target, "--json"}, &output, &error), qPrintable(error));
        QVERIFY(QJsonDocument::fromJson(output).object().value("valid").toBool());
        QVERIFY(!Storage::run(binary, {"apply", "--seed", "#ff66aa", "--json"}, &output, &error));
        QVERIFY(error.contains("--yes"));
    }
};

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Material");
    AetherTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "aether_tests.moc"
