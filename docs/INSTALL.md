# Installing White Balance for OBS

Download the file for your platform from the latest release on the [Releases page](https://github.com/Armitage64/obs-whitebalance/releases). Each release's notes list the SHA-256 checksum of every file. To verify a download, run `shasum -a 256 <file>` on macOS or `Get-FileHash <file>` in PowerShell, and compare.

| | |
|---|---|
| macOS | 12 or later, Apple Silicon and Intel (universal) |
| Windows | 10 or 11, 64-bit |
| OBS Studio | 31 or later (tested on 33.0.1). On Windows, see the note for OBS 31–32 below. |

## macOS
1. Quit OBS.
2. Extract the macOS archive (`obs-whitebalance-<version>-macos-universal.tar.xz` or `.zip`). You get `obs-whitebalance.plugin`.
3. Copy it into your OBS plugins folder (create the folder if needed). To update, replace the existing copy.
   ```sh
   mkdir -p ~/Library/Application\ Support/obs-studio/plugins
   cp -R obs-whitebalance.plugin ~/Library/Application\ Support/obs-studio/plugins/
   ```
   In Finder: **Go → Go to Folder…** → `~/Library/Application Support/obs-studio/plugins`.
4. Clear the download quarantine flag. The plugin is not notarized by Apple, so macOS blocks it otherwise:
   ```sh
   xattr -dr com.apple.quarantine ~/Library/Application\ Support/obs-studio/plugins/obs-whitebalance.plugin
   ```

## Windows
1. Quit OBS.
2. Extract `obs-whitebalance-<version>-windows-x64.zip`. It contains a folder named `obs-whitebalance`.
3. Copy that `obs-whitebalance` folder into `C:\ProgramData\obs-studio\plugins\`. Create the `obs-studio` and `plugins` folders if they don't exist. To update, replace the existing folder. The result should be:
   ```
   C:\ProgramData\obs-studio\plugins\obs-whitebalance\obs-whitebalance.dll
   C:\ProgramData\obs-studio\plugins\obs-whitebalance\data\white_balance.effect
   C:\ProgramData\obs-studio\plugins\obs-whitebalance\data\locale\en-US.ini
   ```
   - `ProgramData` (one word) is **not** `Program Files`. It's hidden by default: type `C:\ProgramData` into Explorer's address bar. Copying there needs administrator approval.
   - Don't put the plugin in OBS's program folder (`C:\Program Files\obs-studio\…`), and don't create a `plugins` folder there. OBS never looks in such a folder. Copying the DLL into `C:\Program Files\obs-studio\obs-plugins\64bit\` does load it, but OBS's Plugin Manager flags that as a **Legacy** location that will stop working in a future release.
   - Install only one copy. If an older copy exists in a legacy location, remove it.
   - **OBS 31 or 32:** these versions only look in a `bin\64bit` subfolder. Create `obs-whitebalance\bin\64bit\` and move `obs-whitebalance.dll` (and `.pdb`) into it. OBS 33 still loads that layout but marks it **Legacy**.

## Check it loaded
Start OBS. Right-click a video source → **Filters** → **+** → **White Balance**.

You can also go to **Help → Log Files → View Current Log** and look for `[obs-whitebalance] plugin loaded successfully (version …)`.

## Use
1. Click the **eyedropper button** next to **Sample Size**.
2. In the window that opens, click a neutral gray or white area. An 18% gray card is ideal. Drag instead to average a rectangle.
3. Fine-tune **Temperature** and **Tint** with the fields in that window or the sliders in the filter panel.
4. **OK** keeps the values. **Cancel** restores the previous ones.

## Uninstall
Quit OBS, then delete the plugin:
- **macOS:** `~/Library/Application Support/obs-studio/plugins/obs-whitebalance.plugin`
- **Windows:** `C:\ProgramData\obs-studio\plugins\obs-whitebalance`

## Troubleshooting
- **Filter not listed:**
  - Check the folder layout above. On macOS the plugin must sit directly in `plugins`, and you must have run the `xattr` command.
  - On Windows, the DLL must be at `C:\ProgramData\obs-studio\plugins\obs-whitebalance\obs-whitebalance.dll` (ProgramData, not Program Files; `bin\64bit\` only for OBS 31–32).
- **Plugin Manager says "installed to a location that will stop working in the future":** the DLL is in a legacy location (`bin\64bit\` or `C:\Program Files\obs-studio\obs-plugins\64bit\`). Move it to the layout above.
  - Then look in the OBS log for `obs-whitebalance` errors.
- **Picker shows black / "sending completely black frames":** the source has no picture, for example a capture card with no signal. Also check you added the filter to the right source; one hidden under another source in the scene isn't visible in the preview.
- **Picker says "Waiting for video…":** the filter must be enabled and the source visible in Preview or Program.
