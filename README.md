
# alo-viewer
[![build](https://github.com/GlyphXTools/alo-viewer/actions/workflows/build.yml/badge.svg)](https://github.com/GlyphXTools/alo-viewer/actions/workflows/build.yml?query=branch%3Amaster)

# About
3D Viewer for Glyph's ALO files (Star Wars: Empire at War / Forces of Corruption).

## Requirements
Windows 7 or later (otherwise change Preprocessor Definition to _WIN32_WINNT=0x600)

---

# Fork: headless capture (`feature/headless-capture`)

This fork adds a **headless render-to-PNG mode** so an `.alo` can be rendered
**without any GUI interaction** and the result saved to an image, then the app
exits. It exists to make Alamo rendering **scriptable** — for CI, automated
checks, and AI-assisted tooling.

## Why

Format-level round-trip checks (re-parsing an `.alo`, byte-diffing) prove a file
is *well-formed*, but they cannot prove it is *engine-valid* — that the real
Alamo shaders compile for it, its textures resolve, and it actually displays.
The only ground truth for that has historically been "open it in AloViewer and
look." This mode turns that visual check into a command you can run in a script.

It was built to verify the [max2alamo](https://github.com/DrKnickers/max2alamo-2024)
`alo_material` editor — a headless tool that retextures / re-shaders an `.alo` by
chunk surgery. After an edit, this mode renders the result and confirms the
engine loads the new texture and the model still shades, which no byte-oracle
can establish.

## Usage

```
AloViewer.exe <model.alo> --capture <out.png>
              [--camera-azimuth <deg>] [--camera-elevation <deg>] [--camera-distance <units>]
```

- Renders one settled frame of `<model.alo>` to `<out.png>` (PNG), then exits.
- **Exit code:** `0` on success, `1` if the capture could not be written
  (so a script can detect failure).

### Framing

The capture **auto-frames the model by its bounding box**: it looks at the box
center and pulls back far enough to fit the model's bounding sphere in the
field of view, defaulting to a 3/4 view. This means *any* model frames whole and
centered with no per-model tuning. The flags override the default:

- `--camera-azimuth <deg>` — orbit around the model center (0 = front).
- `--camera-elevation <deg>` — angle above the horizontal plane.
- `--camera-distance <units>` — explicit distance (overrides the auto-fit).

### Deterministic renders + pixel-diff verification

Before grabbing the frame, the capture renders a short **warmup** so the saved
image is fully lit and shader-compiled (the first frame is otherwise often
near-black). As a result, **identical input produces a byte-identical PNG**.

That determinism enables a strong verification pattern: render a baseline and an
edited model with the same framing and **pixel-diff them**. The noise floor
(the same file rendered twice) is exactly zero, so any changed pixels localize
*precisely* the visible effect of the edit. (Example: a `BaseTexture` rebind on
one submesh changes only that submesh's pixels, on a black background.)

### Diagnostics (`stderr`)

While loading, the capture prints per-resource gate lines — the programmatic
acceptance signal, independent of camera framing:

- `[shader-gate] effect loaded OK: <Name.fx>` / `selected technique: <t>` /
  `FALLBACK to placeholder (missing/failed): <Name.fx>` — shader resolution.
- `[tex-gate] param=<Slot> file=<Name> loaded=<0|1>` — texture resolution.
  **`loaded=1`** means the engine resolved and created the texture;
  **`loaded=0`** means it was missing (renders white/placeholder). This is the
  key signal that an edited/inserted texture binding actually took effect.

### Asset resolution

Textures and shaders resolve from the active game/mod base directory (the
EaW/FoC `DATA` tree the loaded model belongs to). Run against a real install or
an extracted `DATA` directory so the model's textures are found (otherwise the
`tex-gate` lines report `loaded=0`).

### Known limitations

- Headless launches can occasionally hang during scene setup (DX9 / window-message
  timing). Scripts should run with a timeout and retry (and kill any stuck
  `AloViewer.exe` between attempts).
- A malformed/unsupported `.dds` can hang the texture loader instead of falling
  back — prefer known-good textures when scripting verifications.

## Building

The headless build is produced with the VS2022 BuildTools compiler; see
[`build.bat`](build.bat) for the exact recipe (`msbuild AloViewer.sln`,
`Release`/`x86`, borrowing the header-only `afxres.h` from an MFC-equipped
toolset). The output is `Release/AloViewer.exe`.
