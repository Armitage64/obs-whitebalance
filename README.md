# White Balance Eyedropper for OBS

An OBS Studio video filter that brings Premiere Pro's **Lumetri Color → Basic Correction → WB Selector** to OBS. You click a neutral target (ideally an 18% gray card) with an eyedropper, and the filter sets **Temperature** and **Tint** so that the patch becomes neutral. Sliders with spin boxes let you fine-tune the result.

## Usage
1. Right-click a video source (e.g. *FaceTime HD Camera*) and choose **Filters**. Click **+** and add **White Balance (Eyedropper)**.
2. Click **Pick White Balance (Eyedropper)…**. A window opens with a live, *uncorrected* view of the source.
3. Hover to see a magnified loupe, the sample box and a live RGB / Temperature / Tint readout.
   - **Click** to sample the chosen area (1–31 px square).
   - **Drag** to average a rectangle, such as a whole gray card.
4. The correction shows live in OBS. To fine-tune, use the Temperature/Tint fields in the picker or the sliders in the filter properties.
   - **OK** keeps the values.
   - **Cancel** restores the previous values.
   - **Reset** sets both to 0.

Sign conventions match Lumetri:
- **+Temperature** is warmer (orange); −Temperature is cooler (blue).
- **+Tint** is magenta; −Tint is green.
- Both range from −100 to 100.

The source must be visible in Preview or Program, and the filter enabled, for the picker to receive frames.

## How it works
- Correction is a per-channel gain applied in **linear light**: sRGB is decoded, multiplied, then re-encoded. Rec.709 luminance of neutrals is preserved.
- **Model:**
  - log2 gR = T/100 + M/150
  - log2 gB = −T/100 + M/150
  - log2 gG = −2M/150
  - So ±100 Temperature is ±2 stops of R/B and ±100 Tint is ±2 stops of G vs. magenta.
- **Solving a sample:** the eyedropper averages the sample in linear RGB and solves T and M in closed form so the sample becomes R = G = B.
- **Rejected samples:** too dark (any channel < 0.003 linear) or clipped (≥ 0.95 linear) samples are refused with a message.
- **Rendering:** the filter renders its input into its own texture, which drives both the corrected output and the eyedropper snapshot. The snapshot reaches the CPU through a staging surface, read one frame late so the GPU doesn't stall. Color spaces handled: SRGB, SRGB_16F and 709_EXTENDED (HDR).

## Build (macOS)
```sh
cmake --preset macos
cmake --build --preset macos
cp -R build_macos/RelWithDebInfo/obs-whitebalance.plugin \
  ~/Library/Application\ Support/obs-studio/plugins/
```
The first configure downloads OBS sources, obs-deps and Qt6 (see `buildspec.json`). The output is a universal (arm64 + x86_64) bundle.

### Unit tests (no OBS needed)
```sh
cmake -S tests -B build_tests && cmake --build build_tests && ctest --test-dir build_tests
```

### Manual test card
`tests/graycard_tungsten_cast.png` is an 18% gray card with a known tungsten-like cast (linear gains 1.4 / 1.0 / 0.7).
1. Add it as an Image source and add the filter.
2. Click the center patch.
3. Expect **Temperature ≈ −50.0, Tint ≈ +0.7**. The patch should read R ≈ G ≈ B ≈ 118.

## Notes
- The eyedropper lives in its own window because the OBS plugin API can't intercept clicks on the main preview.
- Scaffolded from [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate). Local changes to the template:
  - `cmake/common/buildnumber.cmake` defaults the build number outside CI.
  - `CMakeLists.txt` drops Qt's `-framework AGL`, which current macOS SDKs no longer ship.
- The UI conventions (grouping, sign tooltips) were inspired by the Lua script *Lumetric Corrector*. No code from it is used.

License: GPL-2.0-or-later.
