# Omarchy Aether development

- This is a standalone C++17 / Qt Quick application. Build with `bin/build` (qmake6 and make), test with `bin/test`, and package with `bin/install`.
- Keep C++ and QML in flat `src/`. C++ owns files, processes, palette logic, and error handling; QML owns layout and interaction. Use QGuiApplication, not QApplication.
- The binary, package, desktop file, and Wayland app ID are `omarchy-aether`. The existing `aether` program belongs to a different installed package and must not be replaced.
- Preserve the native Omarchy image carousel. `Picker` calls its existing IPC, not a modified packaged plugin. All integration with user shortcuts and menus is explicit and opt-in.
- The normal Theme menu discovers the generated theme and `preview.png`. `integrate --yes` overrides the native `appearance.background` action, carrying its native label/icon/aliases forward because Omarchy normalizes missing fields before merging. Preserve comments/unrelated settings and refuse existing custom actions/symlink-managed files. `style` is a submenu alias, not the current native row ID.
- Never change the active desktop theme, keybindings, language, app configuration, or original wallpapers as part of development or tests. Use temporary roots and injected tools. `apply --yes` is a user action, not a build step.
- Read `/usr/share/omarchy` for compatibility, never edit it. Runtime theme generation belongs under the user's Omarchy theme directory and goes through native `omarchy theme set`.
- Matugen must always run with `--dry-run`; a user's Matugen configuration may otherwise execute hooks or set wallpapers.
- Color policy: at least 7:1 body contrast on base, 4.5:1 for every text token on base/raised/selection, 16 distinct ANSI entries, six distinguishable semantic families, and distinct regular/bright pairs. Test the rounded, gamut-mapped sRGB values, not theoretical colors.
- Wallpaper tint may influence surfaces and accents but must not collapse red/green/amber/blue/violet/cyan into a dominant wallpaper hue. Tokyo Night is the diversity reference; no OpenCode-specific theme/configuration belongs here.
- The experimental `generate --hue-rotation 0–1` path is preview/export only. Keep `Palette::create`, native menu generation, and installed defaults unchanged. Never allow the experimental flag on apply/staging/picker commands without a separate approved design.
- Thumbnails are persistent, source-keyed previews. Batches use a shuffled deck, pin the current wallpaper, and stay fixed while browsing. Unique stable aliases avoid Omarchy's basename deduplication. No whole-library scan belongs on the warm-open path.
- Theme watching must survive directory/file replacement. Follow Omarchy's effective rounding, locale, translations, and RTL direction. All visible prose uses qsTr/tr and plain-text rendering.
- Add Qt Test coverage after backend changes. Run the real QML offscreen for interactions and screenshots; use synthetic content, never private files. Keep the Omacut/template MIT attribution.
- Do not commit build output, caches, source wallpapers, generated palettes, or machine paths. The only generated documentation image is an offscreen screenshot with a synthetic landscape.
