# Initial validation — 2026-11-06

Environment: Omarchy 4.5.0, Qt 6.12.0, Matugen 4.2.0, GCC 16.2.1.

## Color policy

The automated tests cover 72 source hues at five-degree intervals, monochrome sources, extreme surface inputs, gamut mapping, final sRGB contrast, semantic hue separation, 16 unique ANSI entries, and regular/bright differences. Collapsed palettes and faint secondary text are explicitly rejected.

Twelve representative static images were downloaded from `JValdivia23/walls` into a temporary validation directory, never into this repository. Categories: abstract, calm, flowers, gruvbox, minimal, monochrome, mountain, nature, nord, outrun, painting, and stalenhag.

All twelve passed:

| Measurement | Observed |
|---|---:|
| Body contrast against the base surface | 9.29–9.45:1 |
| Lowest text contrast across tested surfaces | at least 4.80:1 |
| Closest semantic-color distance | at least 8.87 OKLab distance × 100 |
| Unique ANSI entries | 16/16 for every image |

The six semantic colors remain deliberately more stable than the wallpaper-derived accent. This is intentional: readability and category distinction outrank exact pixel matching.

## Backend and UI

The Qt Test run reports **32 passed, zero failed** (including setup and cleanup). It covers the actual QML offscreen, injected file-chooser interaction, keyboard help, live English/Danish switching, RTL layout, directory-replacing theme changes, cache identity, shuffled non-repeating batches, duplicate image basenames, malformed Matugen output, native picker protocol rejection and disconnect recovery, cancellation, atomic staging, original-file protection, and preservation of transient theme images.

`docs/screenshot.png` is captured from that real QML with a procedural landscape and the default fallback accent. It contains no user's files or desktop content.

The carousel protocol is tested with an injected shell and checked against the installed Omarchy 4.5 image-selector IPC source. Automated tests do not open the user's real carousel, change keybindings, or apply a theme. A complete end-to-end desktop apply remains a user-approved acceptance test.

These are opaque sRGB measurements. Applications can derive additional colors or introduce opacity; the generator cannot certify every rendered application state or every color-vision deficiency. Manual review of the system-derived application colors remains part of acceptance.

## Native-menu integration in 0.2.0

- The expanded local run reports **43 passed, zero failed**. The full user collection contains 1,642 supported images, and every indexed thumbnail was generated successfully. No source images or full-library caches are committed to the project.
- Native theme registration includes a normal `preview.png` containing the selected wallpaper and its ANSI swatches, alongside `colors.toml` and its wallpaper. It does not replace the Theme menu action.
- The Background override targets the installed `appearance.background` row, not the historical `style` alias. Native metadata is copied forward because Omarchy normalizes missing fields before merging.
- The JSONC installer is tested for comments, literal comment-looking text inside strings, trailing commas, missing separators, backups, idempotence, malformed input, and protection of existing custom actions/symlinks.
- The actual installed Omarchy JavaScript menu model is evaluated in Qt's JavaScript engine to verify the label, icon, aliases, and action after merging. That compatibility check is skipped on CI hosts without Omarchy; the portable metadata checks still run.
- The native Background CLI is exercised with isolated state/config/cache roots and injected executables. Non-Aether themes follow the stock selector/setter path, and a theme change during Aether selection prevents unintended reactivation.
