# Adventures with Chickens Remastered

This repository contains the source code for a clean C++20 reimplementation of
RockSolid Software's 1998 Windows game *Adventures with Chickens*.

Play it in your browser at
[adventures-with-chickens.pages.dev](https://adventures-with-chickens.pages.dev).

## Assets

The original graphics, sound, music, and level data are intentionally not included in
this repository. The source expects those files beneath `assets/` when building, so this
repository does not produce a complete game build on its own.

## Architecture

The game uses plain C++20, SDL3, direct mode/submode switches, a 60 Hz fixed simulation
step, and presentation up to 144 Hz. The implementation is native widescreen rather than
an emulator or a scaled 640x480 framebuffer.

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
