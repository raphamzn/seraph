# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Test

```bash
# Build
cmake -B build && cmake --build build

# Run
./build/src/seraph

# Run all tests
ctest --test-dir build

# Run a single test
./build/tests/tst_configmanager
```

Tests use Qt6::Test (QCOMPARE, QSignalSpy). Test sources are in `tests/tst_*.cpp` — one per backend class.

## Architecture

Seraph is a Qt6/QML file manager with three layers:

**QML Frontend** (`src/qml/`) — All rendering. `Main.qml` is the root that wires tab state, selection, and keyboard shortcuts. `FileViewContainer.qml` switches between grid/list/detailed views. `Theme.qml` is a QML singleton providing colors from the active TOML theme.

**C++ Backend** (`src/models/`, `src/services/`, `src/providers/`) — Exposed to QML as context properties set in `main.cpp`. Models (FileSystemModel, TabListModel, BookmarkModel, DeviceModel) are all QAbstractListModel subclasses with custom roles. Services (ConfigManager, ThemeLoader, FileOperations, ClipboardManager) manage state and async operations. SystemAppearance reports the desktop light/dark preference (xdg-desktop-portal `org.freedesktop.appearance`, with QStyleHints as fallback) and drives ThemeLoader when `theme = "auto"`.

**System Layer** — FileOperations spawns gio/xdg-open via QProcess. DeviceModel monitors UDisks2 over DBus. Assumes Wayland (wl-copy for clipboard).

### Data flow

QML action → Q_INVOKABLE C++ method → model property change → QML property binding re-renders view. FileSystemModel watches directories via QFileSystemWatcher for automatic reload.

### Key conventions

- `IconProvider` resolves an icon through the theme's own `Inherits=` chain, then `hicolor`, then the MIME type's generic icon. It deliberately does NOT consult a hardcoded list of popular themes: that made an unrelated package install silently repaint the app
- Provider URLs carrying a file path go through the `MediaUrl` QML singleton (`src/qml/theme/MediaUrl.qml`), which percent-encodes each path segment. A raw path pasted into a source string breaks on any filename containing `?` or `%` — the image just never appears, with no error
- Every `image://icon/` URL is built from `Theme.iconQuery` (in `src/qml/theme/Theme.qml`), which carries the icon pack and the folder tint. The query doubles as the cache key — QML caches provider images by URL, so it changing is what repaints icons
- Models expose data via `roleNames()` mapping enums to QML-accessible names (e.g., `FileNameRole` → `"fileName"`)
- QML components communicate upward via signals (fileActivated, contextMenuRequested), downward via property bindings
- Config lives at `~/.config/seraph/config.toml` (TOML format); theme files in `~/.config/seraph/themes/` (user, wins) then `themes/` (bundled). ThemeLoader watches the active theme file, so rewriting it recolors the running app
- All async file I/O through QProcess to avoid blocking the GUI thread
- Markdown in the quick preview is parsed by `MarkdownRenderer` (`src/services/markdownrenderer.cpp`) with Qt's own CommonMark parser into typed blocks, then drawn by `MarkdownView.qml`/`MarkdownBlock.qml` with native delegates. Qt reports every fenced line, hard line break, and list item as an identical-looking block, so the renderer re-splits fences and marks hard breaks by scanning the source text first. Fenced code goes through `bat` (shared helper `BatHighlighter`) when installed; remote images are never fetched

## Commit Rules

Never add Co-Authored-By lines to commits.

## Shared Submodules

- `src/qml/icons/` → [quill-icons](https://github.com/soyeb-jim285/quill-icons) — 60 PathSvg icons (Lucide-derived, ISC/MIT)
- `src/qml/Quill/` → [quill](https://github.com/soyeb-jim285/quill) — Themed QML component library (Button, TextField, Card, Tabs, Dropdown, etc.)

Quill's `Theme.qml` singleton is bridged from Seraph's theme in `Main.qml` `Component.onCompleted`. The directory must be uppercase `Quill/` to match the QML module name.

## Packaging & Distribution

- **Arch package**: `PKGBUILD` in the repo root. Published on the AUR as `seraph-git`, and built locally with `makepkg -si`.
- **AppImage**: GitHub Actions builds on `v*` tags (`.github/workflows/build.yml`)
- **Desktop entry + icon**: `dist/seraph.desktop`, `dist/seraph.svg`

### Installing a local build

`makepkg -si` from the repo root produces and installs `seraph-git`. The PKGBUILD **clones `main` from GitHub** rather than using the working tree, so it packages what has been pushed — commit and push before building, or the package will not contain the change under test.

### The AUR package

`seraph-git` is on the AUR (published 2026-08-29), so `paru -S seraph-git` works. Its repo is a separate git repo holding only `PKGBUILD` + `.SRCINFO` — build instructions, not source code. There is no local clone of it and you should not need one: `.github/workflows/aur.yml` publishes on its own, after a green `Build` of `main` and on a 6-hourly schedule. Because the PKGBUILD clones `main` at build time, a code change needs nothing done to the AUR at all; only dependency, build-step or install-path changes require touching the PKGBUILD.

`.SRCINFO` is the metadata the AUR and `paru`/`yay` actually read, and fields in it are derived from `pkgver` (`provides=`), so the workflow regenerates it with `makepkg --printsrcinfo` in an Arch container rather than patching lines. Keep the copy in this repo regenerated whenever the PKGBUILD changes — the workflow does not read it, but it is what a reader sees.

Two of the three `source=` entries point at soyeb-jim285's quill and quill-icons. If either is renamed or archived, the AUR build breaks for everyone with no change on this side.

### SERAPH_DATA_DIR

`SERAPH_DATA_DIR` (CMake cache var) controls where the binary finds themes and QML at runtime. Defaults to `CMAKE_SOURCE_DIR` for dev. PKGBUILD sets it to `/usr/share/seraph`. Separate from `SERAPH_SOURCE_DIR` which is always the build source dir (needed for `loadFromModule`).

## Dependencies

Qt6 modules: Core, Gui, Qml, Quick, QuickControls2, DBus, Widgets, Svg, SvgWidgets. TOML parsing via header-only `third_party/toml.hpp`. Runtime CLI tools: gio, xdg-open, wl-copy (optional; warns if missing).
