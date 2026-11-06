# Gander

Quick Look for Linux. Select a file in Nautilus, press **Space**, and see it:
photos, video, music with its cover art, PDFs, Word/Excel/PowerPoint, Markdown,
code, CSV, folders, archives and fonts. Space again closes it.

![Gander previewing a song](docs/screenshot.png)

Gander replaces Sushi, GNOME's previewer, by taking over the D-Bus name
Nautilus calls on Space (`org.gnome.NautilusPreviewer`). Nautilus needs no
plugin and no patch. It's built for Omarchy (Hyprland, Qt, follows the theme
accent and language) and works on any wlroots-style Wayland desktop.

## Why it's fast

- **Always warm.** A small daemon starts with your session and keeps its
  window, GPU context and every viewer ready. Pressing Space only maps a
  surface. On a mid-range iGPU the first frame lands in 8 to 20 ms.
- **Paints before it decodes.** The window takes its final shape from the file
  header alone. Nautilus's existing thumbnail fills the first frame, and the
  real image replaces it a moment later.
- **Decodes at screen size.** JPEGs decode at reduced scale (about 100 ms for
  24 MP), and the full resolution loads only when you zoom in.
- **Reads ahead.** While you look at one photo, its neighbours decode in the
  background. Stepping to the next one takes about 1 ms.
- **Converts Office files once.** LibreOffice turns a document into a PDF in
  its own private profile, so it never clashes with an open LibreOffice
  window. The result is cached by path, size and modification time. While it
  converts, the document's text shows straight from the file.
- **Floats above everything.** The preview is a layer-shell overlay, so
  nothing tiles, reflows or animates out of turn.

## Keys

| Key | Action |
| --- | --- |
| Space, Esc, Q | Close |
| ← → ↑ ↓ | Previous or next file (in Nautilus, moves its selection) |
| Enter | Open in the default app |
| F | Fill the screen |
| + − 0, wheel, pinch, double-click | Zoom |
| K / J / L | Play or pause, back 5 s, forward 5 s |
| M | Mute |
| PgUp PgDn Home End | Scroll documents |
| Delete, F2 | Move to trash, rename (from Nautilus) |
| ? | Show the keys |

## Install

```sh
bin/install
```

This builds the package and installs it with `makepkg -si`. Pacman asks to
remove `sushi`; Gander provides the same service. The script then restarts
`gander.service`.

For Hyprland, also add the line in `/usr/share/gander/hyprland.lua` to your
config. It skips the compositor's layer fade; Gander animates its own opening.

Office previews need `libreoffice-fresh`. HEIC, AVIF and RAW photos need
`kimageformats`.

## From anywhere else

```sh
gander FILE     # preview FILE; run it again to close
gander close
```

The command is a thin D-Bus client and returns at once, so it suits a
keybinding or a terminal file manager. For yazi:

```toml
[[manager.prepend_keymap]]
on = "<Space>"
run = 'shell -- gander "$0"'
```

## Develop

```sh
bin/build
bin/test
GANDER_TRACE=1 QT_FORCE_STDERR_LOGGING=1 build/gander daemon   # timings per step
QT_QPA_PLATFORM=offscreen GANDER_SNAPSHOT=/tmp/shot.png build/gander daemon
```

The source is in `src/`:

| Files | Role |
| --- | --- |
| `inspector` | The fast first look at a file |
| `previewer` | State, D-Bus requests and the worker-thread loaders |
| `overlay` | The layer shell |
| `audio`, `pdf`, `office`, `textloader`, `listing` | One per format family |
| `*.qml` | The window and its viewers |

## License

MIT. Theme and language following are adapted from Omacut and Monologue
(David Heinemeier Hansson).
