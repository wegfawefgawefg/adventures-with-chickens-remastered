# Adventures with Chickens Remastered

This repository is the public source-code mirror for a clean C++20 reimplementation of
RockSolid Software's 1998 Windows game *Adventures with Chickens*.

Play the authorized browser build at
[adventures-with-chickens.pages.dev](https://adventures-with-chickens.pages.dev).

## Source-only mirror

This repository contains the reverse-engineered implementation, build configuration,
and browser-shell source. It intentionally does **not** contain the original graphics,
sound, music, or level data, and is therefore not a self-contained game distribution.

Development happens in a private canonical repository that holds the authorized asset
set. This public tree is rebuilt automatically from an explicit source allowlist; do not
expect direct commits here to survive the next sync.

## Architecture

The game uses plain C++20, SDL3, direct mode/submode switches, a 60 Hz fixed simulation
step, and presentation up to 144 Hz. The implementation is native widescreen rather than
an emulator or a scaled 640x480 framebuffer.

The source expects the original asset tree at `assets/` when building. Those files are
not offered by this repository. You are responsible for supplying only material you
have the right to use.

## Source layout

- `src/` contains the native and browser game implementation;
- `site/` contains the browser shell, without generated game or asset bundles;
- `scripts/` contains local native/web build helpers;
- `CMakeLists.txt` and `CMakePresets.json` define the C++ build.

## License boundary

The reverse-engineered source code in this repository is available under the MIT
License. See [LICENSE](LICENSE) and [NOTICE.md](NOTICE.md).

The MIT License does not cover the *Adventures with Chickens* name, original artwork,
audio, music, levels, or other original game material. Those materials are not included
here and remain the property of their respective rights holders.
