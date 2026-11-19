# Changelog

All notable changes to this project are documented here. Versions follow [Semantic Versioning](https://semver.org/). Downloads are on the [Releases page](https://github.com/Armitage64/obs-whitebalance/releases).

## [Unreleased]
### Added
- Windows (x64) support and build instructions.
- GitHub Actions CI building macOS, Windows and Ubuntu, with a format check. Version tags create a draft release with all packages and checksums.

### Changed
- Windows package uses the OBS 33 plugin layout (`obs-whitebalance\obs-whitebalance.dll` with no `bin\64bit`), so OBS's Plugin Manager no longer marks the plugin **Legacy**.
- Windows CI artifact is the plain package zip instead of a zip inside a zip.
- Picker readout uses the system monospace font (was macOS-only Menlo).
- Install instructions moved to `docs/INSTALL.md` and now cover Windows. Release builds are distributed only through GitHub Releases, not committed to the repository.

## [0.1.0] - 2026-11-18
First release.

### Added
- **White Balance** video filter for OBS Studio (macOS universal: Apple Silicon and Intel, OBS 31+).
- Eyedropper window: live uncorrected view of the source, magnified close-up, and a live RGB / Temperature / Tint readout. Click to sample a 1–31 px square, or drag to average a rectangle.
- Temperature and Tint controls (sliders and spin boxes, −100…100), using Lumetri's sign conventions.
- Correction applied as linear-light per-channel gains that preserve the brightness of grays, solved exactly from the sample.
- Support for SDR, 16-bit float and HDR (709 extended) sources.
- Warnings for samples that are too dark or overexposed, and for sources that send only black frames.
- English and German translations.

[Unreleased]: https://github.com/Armitage64/obs-whitebalance/compare/v0.1...HEAD
[0.1.0]: https://github.com/Armitage64/obs-whitebalance/releases/tag/v0.1
