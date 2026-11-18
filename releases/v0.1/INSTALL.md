# White Balance for OBS — v0.1 install

**File:** `obs-whitebalance-0.1-macos-universal.zip`
**SHA-256:** `267cde5a3cf19b8ce581d67cc568e96deef6bc993c17d1b47a2a54927e2c0be9`

| | |
|---|---|
| Platform | macOS 12 or later, Apple Silicon and Intel (universal binary) |
| OBS Studio | 31 or later (tested on 33.0.1) |

## Install
1. Quit OBS.
2. Unzip `obs-whitebalance-0.1-macos-universal.zip`. You get `obs-whitebalance.plugin`.
3. Copy it into your OBS plugins folder (create the folder if it doesn't exist):
   ```sh
   mkdir -p ~/Library/Application\ Support/obs-studio/plugins
   cp -R obs-whitebalance.plugin ~/Library/Application\ Support/obs-studio/plugins/
   ```
   In Finder: **Go → Go to Folder…** → `~/Library/Application Support/obs-studio/plugins`.
4. Clear the download quarantine flag. The plugin is ad-hoc signed, not notarized by Apple, so macOS blocks it if it came from a browser download:
   ```sh
   xattr -dr com.apple.quarantine ~/Library/Application\ Support/obs-studio/plugins/obs-whitebalance.plugin
   ```
5. Start OBS. Right-click a video source → **Filters** → **+** → **White Balance**.

To check it loaded, go to **Help → Log Files → View Current Log**. Look for `[obs-whitebalance] plugin loaded successfully (version 0.1.0)`.

## Use
1. Click the **eyedropper button** next to **Sample Size**.
2. In the window that opens, click a neutral gray or white area. An 18% gray card is ideal. Drag instead to average a rectangle.
3. Fine-tune **Temperature** and **Tint** with the fields in that window or the sliders in the filter panel.
4. **OK** keeps the values. **Cancel** restores the previous ones.

## Uninstall
Quit OBS and delete `~/Library/Application Support/obs-studio/plugins/obs-whitebalance.plugin`.

## Troubleshooting
- **Filter not listed:** check that the plugin sits directly in the `plugins` folder, not in a subfolder. Check that you ran the `xattr` command. Then look in the OBS log for `obs-whitebalance` errors.
- **Picker shows black / "sending completely black frames":** the source has no picture, for example a capture card with no signal. Also check you added the filter to the right source; one hidden under another source in the scene isn't visible in the preview.
- **Picker says "Waiting for video…":** the filter must be enabled and the source visible in Preview or Program.
