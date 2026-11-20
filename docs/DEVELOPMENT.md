# Development

How to build, test and release White Balance Eyedropper, plus notes on implementation details. For what the plugin does and how to use it, see the [README](../README.md). Paths below are relative to the repository root.

## Build
The first configure downloads OBS sources, obs-deps and Qt6 (see `buildspec.json`).

**macOS** (Xcode). The output is a universal (arm64 + x86_64) bundle:
```sh
cmake --preset macos
cmake --build --preset macos
cp -R build_macos/RelWithDebInfo/obs-whitebalance.plugin \
  ~/Library/Application\ Support/obs-studio/plugins/
```

**Windows** (Visual Studio 2022 with the "Desktop development with C++" workload, CMake 3.28+):
```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
cmake --install build_x64 --prefix release --config RelWithDebInfo
```
Then copy `release\obs-whitebalance` into `C:\ProgramData\obs-studio\plugins\`.

**Linux** (Ubuntu 24.04, **untested**): `cmake --preset ubuntu-x86_64 && cmake --build --preset ubuntu-x86_64`. This needs the OBS and Qt6 development packages; see `.github/scripts/utils.zsh/setup_ubuntu`. CI builds Linux only to check the code compiles. No Linux packages are released, because the plugin hasn't been tested on Linux, and Ubuntu's own OBS packages (30.x) are older than the supported OBS 31+.

## Continuous integration
GitHub Actions (`.github/workflows`, from obs-plugintemplate) builds macOS, Windows and Ubuntu (compile check only) on every push to `main`. It also checks formatting (clang-format 19, gersemi).

Pushing a version tag such as `v0.2.0` also creates a **draft** GitHub release with the macOS and Windows packages and checksums attached. Review the draft and publish it. Bump `version` in `buildspec.json` and update `CHANGELOG.md` before tagging.

## Unit tests (no OBS needed)
```sh
cmake -S tests -B build_tests && cmake --build build_tests && ctest --test-dir build_tests
```

## Manual test card
`tests/graycard_tungsten_cast.png` is an 18% gray card with a known tungsten-like cast (linear gains 1.4 / 1.0 / 0.7).
1. Add it as an Image source and add the filter.
2. Click the center patch.
3. Expect **Temperature ≈ −50.0, Tint ≈ +0.7**. The patch should read R ≈ G ≈ B ≈ 118.

## Notes
- The eyedropper lives in its own window because the OBS plugin API can't intercept clicks on the main preview.
- **Properties-panel layout.** OBS's generic properties panel has no API for button icons, field widths or putting two properties on one row. `src/wb-ui.cpp` installs a Qt event filter that recognizes this plugin's widgets as OBS builds them: the pick button by its label, the Sample Size dropdown by its tooltip. The filter then:
  - turns the button into an eyedropper icon (drawn in code, following the theme's text color);
  - stops the dropdown from stretching;
  - moves the button onto the dropdown's row.

  If a future OBS changes how it builds that panel, these widgets fall back to OBS's default look. Nothing breaks.
- **Logging.** The OBS log records each eyedropper capture (source, size, color space, first-frame stats) under `[obs-whitebalance]`.
- Scaffolded from [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate). Local changes to the template:
  - `cmake/common/buildnumber.cmake` defaults the build number outside CI.
  - `CMakeLists.txt` drops Qt's `-framework AGL`, which current macOS SDKs no longer ship.
- The UI conventions (grouping, sign tooltips) were inspired by the Lua script *Lumetric Corrector*. No code from it is used.
