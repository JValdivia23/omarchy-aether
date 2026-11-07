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

## Preview-only hue rotation experiment

The experimental run reports **52 passed, zero failed**, including the unchanged QML interaction/language checks. Added coverage includes 72 source hues with two strengths and two wallpaper targets, hue-wrap handling, actual rounded sRGB contrast and separation, coordinated regular/bright rotation, local influence, chroma/lightness bounds, neutral/transparent images, deterministic multi-hue sampling, and rejection of invalid/nonfinite strengths. The CLI rejects the experimental flag on apply, staging, native picker, and integration commands before they can change configuration.

Three wallpapers from the existing library were compared at strengths 0, 0.5, and 1: a cool blue-on-slate graphic, a plum/amber mountain sunset, and warm desert dunes. The default output matched the installed production binary exactly for each image; the cool comparison baseline also matched the active system palette. All experiment caches, palettes, and visual comparisons stay outside the repository, and neither the installed executable nor active theme was modified.

| Wallpaper | Body contrast (all strengths) | Lowest text contrast (all strengths) | Semantic distance, baseline → gentle → stronger |
|---|---:|---:|---:|
| Cool graphic | 9.41:1 | 4.83:1 | 9.28 → 8.37 → 7.56 |
| Mountain sunset | 9.27:1 | 4.81:1 | 9.00 → 9.87 → 10.41 |
| Desert dunes | 9.25:1 | 4.81:1 | 9.05 → 9.05 → 9.05 |

Every palette has 16 unique ANSI entries. Distances are Euclidean OKLab × 100, with a minimum accepted value of 7.5. Full-strength rotation is safety-limited: approximately 8° for cyan/violet in the cool graphic and 9° for red/amber in the dunes. This is deliberately a modest adjustment, not evidence that all colors can be pulled arbitrarily close to a single wallpaper hue without losing diversity.

An additional 12-category wallpaper sweep (abstract, calm, flowers, gruvbox, minimal, monochrome, mountain, nature, nord, outrun, painting, and stalenhag) passed all **24 generated palettes** at strengths 0.5 and 1. Observed minimums were 9.28:1 body contrast, 4.80:1 text contrast, and 7.51 semantic distance. The largest actual rotation in this sweep was 22.85°. These are separate real-image checks in addition to the 52 Qt Test results.
