<div align="center">

<img src="dist/io.github.raphamzn.Seraph.svg" width="96" alt="Seraph logo"/>

# Seraph

**A fast, keyboard-friendly file manager for Hyprland and Wayland desktops.**

[![License](https://img.shields.io/github/license/raphamzn/seraph?style=flat-square)](LICENSE)
[![Build](https://img.shields.io/github/actions/workflow/status/raphamzn/seraph/build.yml?style=flat-square)](https://github.com/raphamzn/seraph/actions)

</div>

---

Seraph is a Qt6/QML file manager designed to feel native on Hyprland: lightweight, themeable, and built around fast keyboard navigation. It pairs a polished UI with the practical features power users expect — Miller column view, kinetic scrolling, drag & drop, async operations, rich previews, and a TOML-based theme system.

> **Seraph is a fork of [HyprFM](https://github.com/soyeb-jim285/hyprfm) by Soyeb Pervez Jim** (MIT). It carries the original work forward under a new name with an independent roadmap. See [Credits](#-credits).

<div align="center">

![Grid view](docs/screenshots/grid-view.png)
*Grid view with built-in icon set, themed sidebar, and live preview blur*

</div>

---

## ✨ Features

### Views

- **Grid view** with adjustable column count (`Ctrl+Scroll` to zoom)
- **Detailed view** with sortable columns, image/video thumbnails, and folder item counts
- **Miller columns** (`Ctrl+2`) — parent · current · live preview, the macOS Finder favorite
- **Image and video thumbnails** in detailed and Miller views
- **Quick preview** (`Space`) — full-screen overlay for images, video, PDFs, text, with metadata sidebar
- **Split pane** (`F3`) — work in two directories side by side

<div align="center">

![Miller view](docs/screenshots/miller-view.png)
*Miller column view with rich text preview and syntax highlighting*

</div>

### Navigation & input

- **Full keyboard navigation** — arrows, vim-friendly shortcuts, type-ahead search
- **Tabs** with independent history per pane
- **Path bar** with breadcrumbs and inline editing (`Ctrl+L`)
- **Bookmarks sidebar** with drag-to-reorder and udisks2 device mounting
- **Kinetic wheel scrolling** with momentum and rubber-band overscroll
- **Rubber-band selection** in all views

### File operations

- **Async copy / move** via `rsync` and `gio` with live progress, speed, ETA, and pause
- **Drag & drop** between panes, tabs, and external apps (Wayland-native)
- **Trash** with restore (XDG-compliant)
- **Bulk rename** with regex find/replace
- **Compress / extract** archives
- **Open With** dialog populated from `.desktop` entries
- **Undo/redo** for file operations

### Look & feel

- **TOML themes** with live reload (Catppuccin Mocha by default)
- **Built-in SVG icon set** (60+ Lucide-style icons rendered via Qt Shapes)
- **Configurable corner radius**, fonts, animation duration
- **Wayland compositor blur** on Hyprland plus native KWin blur on KDE Plasma

### Integrations

- **udisks2** mount/unmount of removable drives
- **gvfs / gio** for SFTP, SMB, MTP, trash, etc.
- **Git status overlays** in file lists (modified, staged, untracked, …)
- **wl-clipboard** for system clipboard
- **bat** for syntax-highlighted text previews
- **ffmpeg** for video poster thumbnails
- **Poppler** for PDF page previews

<div align="center">

![Quick preview](docs/screenshots/quick-preview.png)
*Quick preview overlay (Space) — image preview with full metadata sidebar*

</div>

---

## 📦 Installation

### Build from source

```bash
git clone --recursive https://github.com/raphamzn/seraph.git
cd seraph
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF
cmake --build build --parallel
./build/src/seraph
```

> **Note:** the `--recursive` flag is important — Seraph uses Git submodules for the [Quill](https://github.com/soyeb-jim285/quill) component library and the [quill-icons](https://github.com/soyeb-jim285/quill-icons) icon set (both from the upstream HyprFM project).

#### Dependencies

| | Packages |
|---|---|
| **Required (build)** | `cmake`, `ninja`, `qt6-base`, `qt6-declarative`, `qt6-svg` |
| **Required (runtime)** | `qt6-base`, `qt6-declarative`, `qt6-svg`, `qt6-wayland`, `glib2`, `fd`, `rsync`, `xdg-utils` |
| **Optional** | `kwindowsystem` / `KF6WindowSystem` (native KDE blur), `wl-clipboard` (clipboard), `bat` (syntax highlighting), `gvfs` (remote filesystems), `gvfs-smb` (SMB), `ffmpeg` (video thumbnails), `udisks2` (device mounting), `poppler-qt6` (PDF previews) |

### Packaged builds

AppImage, AUR (`seraph-git`), and Flatpak packaging live in this repo (`.github/workflows/build.yml`, `PKGBUILD`, the Flatpak manifest) and are wired to build on `v*` tags. They point at `raphamzn/seraph` and will produce downloadable artifacts once the first release is tagged and the publishing infrastructure (release repo / signing keys) is configured.

---

## ⌨️ Keyboard shortcuts

### Navigation

| Shortcut | Action |
|----------|--------|
| `Return` / `Double-click` | Open file or directory |
| `Backspace` / `Alt+Up` | Parent directory |
| `Alt+Left` / `Alt+Right` | Back / Forward in history |
| `Ctrl+L` | Focus path bar |
| `Ctrl+F` | Search |
| `Type any letter` | Type-ahead jump to file |

### Views

| Shortcut | Action |
|----------|--------|
| `Ctrl+1` | Grid view |
| `Ctrl+2` | Miller column view |
| `Ctrl+3` | Detailed view |
| `Ctrl+Scroll` | Zoom (icon size or row height) |
| `Space` | Quick preview |
| `F3` | Toggle split pane |
| `F9` | Toggle sidebar |
| `Ctrl+H` | Toggle hidden files |

### Tabs

| Shortcut | Action |
|----------|--------|
| `Ctrl+T` | New tab |
| `Ctrl+W` | Close tab |
| `Ctrl+Shift+T` | Reopen closed tab |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | Cycle tabs |

### File operations

| Shortcut | Action |
|----------|--------|
| `Ctrl+C` / `Ctrl+X` / `Ctrl+V` | Copy / Cut / Paste |
| `Ctrl+A` | Select all |
| `Ctrl+Z` / `Ctrl+Shift+Z` | Undo / Redo |
| `F2` | Rename |
| `Delete` | Move to trash |
| `Shift+Delete` | Permanent delete |
| `Ctrl+Shift+N` | New folder |
| `Ctrl+N` | New file |

All shortcuts can be remapped in `~/.config/seraph/config.toml` under the `[shortcuts]` section.

---

## ⚙️ Configuration

Config lives at `~/.config/seraph/config.toml` and is created with sensible defaults on first run. If you're migrating from HyprFM, Seraph copies your existing `~/.config/hyprfm` config automatically on first launch.

```toml
[general]
theme = "catppuccin-mocha"     # filename in themes/ without .toml
icon_theme = "Adwaita"         # system icon theme fallback
builtin_icons = true           # use bundled SVG icons
default_view = "grid"          # grid | detailed | miller
show_hidden = false
sort_by = "name"               # name | size | modified | type
sort_ascending = true

[sidebar]
position = "left"
width = 200
visible = true

[appearance]
radius_small = 4
radius_medium = 8
radius_large = 12

[bookmarks]
paths = ["~/Documents", "~/Downloads", "~/Pictures", "~/Projects"]

[shortcuts]
# Override any shortcut. Examples:
# rename       = "F2"
# new_tab      = "Ctrl+T"
# miller_view  = "Ctrl+2"
```

---

## 🎨 Theming

Themes are TOML files in `themes/`. Drop a new file there or in `~/.config/seraph/themes/` and reference it from config:

```toml
[colors]
base    = "#1e1e2e"
mantle  = "#181825"
crust   = "#11111b"
surface = "#313244"
overlay = "#45475a"
text    = "#cdd6f4"
subtext = "#bac2de"
muted   = "#6c7086"
accent  = "#89b4fa"
success = "#a6e3a1"
warning = "#f9e2af"
error   = "#f38ba8"
```

Themes reload live on save.

---

## 🧱 Architecture

Seraph is a three-layer Qt6 application:

- **QML frontend** (`src/qml/`) — all rendering. `Main.qml` wires tab state, selection, and shortcuts. Views (`FileGridView`, `FileDetailedView`, `FileMillerView`) are switched by `FileViewContainer`. The [Quill](https://github.com/soyeb-jim285/quill) component library provides themed Buttons, TextFields, Cards, etc.
- **C++ backend** (`src/models/`, `src/services/`, `src/providers/`) — `QAbstractListModel` subclasses for files, tabs, bookmarks, devices. Async services for clipboard, file operations, search, disk usage, previews. Exposed to QML via `setContextProperty`.
- **System layer** — `rsync` / `gio` via `QProcess` for transfers, UDisks2 over DBus for devices, `wl-copy` for clipboard.

See [`CLAUDE.md`](CLAUDE.md) for the full architecture notes.

---

## 🤝 Contributing

Issues and PRs welcome! A few notes:

- Run tests with `ctest --test-dir build` after changes
- Match the existing code style (4-space indent for QML and C++)
- The project uses Git submodules — `git submodule update --init --recursive` after pulling
- AppImage builds are produced automatically on `v*` tags by the GitHub Actions workflow

---

## 🙏 Credits

Seraph is a fork of **[HyprFM](https://github.com/soyeb-jim285/hyprfm)** created by **Soyeb Pervez Jim**, released under the MIT license. Enormous thanks to the original author for the foundation — the core architecture, the QML component set, and the icon library all originate there. The [Quill](https://github.com/soyeb-jim285/quill) component library and [quill-icons](https://github.com/soyeb-jim285/quill-icons) remain upstream projects and are used here as Git submodules.

---

## 📜 License

[MIT](LICENSE) — original work © Soyeb Pervez Jim (HyprFM), fork modifications © Raphael (Seraph).

Built with [Qt 6](https://www.qt.io/) · Icons from [Lucide](https://lucide.dev/) · Inspired by macOS Finder, Nautilus, and Dolphin.
</content>
