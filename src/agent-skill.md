---
name: omarchy-aether
description: Generate, validate, and apply wallpaper-adaptive Omarchy themes with diverse ANSI colors and a cached native wallpaper carousel.
---

# Omarchy Aether

Use `omarchy-aether --help` for the installed commands. Machine-readable commands accept `--json` and exit 0 on success or 1 on failure. Errors go to stderr.

- Inspect without changing the desktop: `omarchy-aether generate --image /absolute/wallpaper.jpg --json`.
- Export a palette: `omarchy-aether generate --image /absolute/wallpaper.jpg --output /path/colors.toml --json`.
- Validate: `omarchy-aether check /path/colors.toml --json`.
- Index a collection explicitly: `omarchy-aether index /path/to/wallpapers --json`.
- Prepare 35 candidates: `omarchy-aether prepare --count 35 --preload --json`.
- Warm the entire indexed library at low priority: `omarchy-aether warm --json`.
- Open the native picker without applying: `omarchy-aether pick --json`.
- Stage a theme for discovery in Omarchy's normal Theme menu: `omarchy-aether apply --image /path/wallpaper.jpg --stage-only --json`.
- Install native Background menu integration only with approval: `omarchy-aether integrate --yes --json`. It backs up and preserves the existing JSONC, carries native row metadata forward, and refuses existing custom Background actions or symlink-managed menu files.
- The native menu action is `omarchy-aether background`: it uses adaptive selection when Aether is active, and delegates to Omarchy's standard selector/setter for other themes.
- Apply only with user approval: `omarchy-aether apply --image /path/wallpaper.jpg --yes --json`.

Never edit `/usr/share/omarchy`, application-specific themes, or wallpaper originals. The generated user theme lives under `~/.config/omarchy/themes/aether/` and is activated through `omarchy theme set aether`. The packaged `aether` application is a different program; do not replace it. Do not activate Aether, rewrite shortcuts, or install agent skills without the user's approval.
