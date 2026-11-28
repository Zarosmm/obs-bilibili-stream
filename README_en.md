# OBS Bilibili Streaming Plugin

A plugin for OBS Studio that simplifies the streaming process on Bilibili platform. Supports QR code login, updating livestream room info, and obtaining RTMP streaming address and key.

## Installation

### Windows

#### Option 1: Installer (recommended)

1.  **Download the installer**: Download the latest `bilibili-stream-for-obs-*-windows-x64.exe` from the [Releases page](https://github.com/Zarosmm/obs-bilibili-stream/releases).
2.  **Close OBS**: Quit OBS Studio before installing.
3.  **Run the installer**: Double-click it (administrator rights required) and choose how OBS is installed:
    - **Installed OBS Studio**: installs to `C:\ProgramData\obs-studio\plugins\bilibili-stream-for-obs`
    - **Portable OBS Studio** (including scoop and other no-install setups): select the OBS root folder (it must contain `bin\64bit\obs64.exe`); the plugin is installed to `<OBS root>\plugins\bilibili-stream-for-obs`. Portable installation requires OBS Studio 33 or later; for older portable builds use the zip package below
4.  **Launch OBS**: Restart OBS Studio, the plugin will load automatically.
5.  **Confirm installation**: Open **Tools → Plugin Manager** in OBS and check that `BILIBILI Stream for OBS` is loaded.
6.  **Uninstall**: Use **Settings → Apps** in Windows to remove `BILIBILI Stream for OBS`. The plugin configuration is stored in the OBS config directory and is kept.

The installer writes both layouts: the new OBS 33+ layout and the legacy OBS 30–32 layout. OBS 33+ prefers the new layout and ignores the duplicate legacy copy; OBS 34 no longer loads the legacy layout.

> The installer is currently unsigned, so SmartScreen may show a warning on first run. Choose **More info** → **Run anyway**.

Silent install:

```bat
bilibili-stream-for-obs-2.1.6-windows-x64.exe /VERYSILENT /SUPPRESSMSGBOXES
bilibili-stream-for-obs-2.1.6-windows-x64.exe /PORTABLE="D:\OBS" /VERYSILENT /SUPPRESSMSGBOXES
```

#### Option 2: Manual installation (zip)

1.  **Download plugin**: Download the latest `bilibili-stream-for-obs-*-windows-x64.zip` from the [Releases page](https://github.com/Zarosmm/obs-bilibili-stream/releases).
2.  **Extract files**: Unzip the downloaded archive.
3.  **Choose the plugins folder**:
    - **Installed OBS Studio**: `C:\ProgramData\obs-studio\plugins`
    - **Portable OBS Studio** (including scoop and other no-install setups): the `plugins` folder in the OBS root directory, next to `bin`. For example, if `obs64.exe` is at `X:\xxx\obs-studio\bin\64bit\`, the plugins folder is `X:\xxx\obs-studio\plugins`.
      > Portable builds **ignore** plugins placed in `C:\ProgramData\obs-studio\plugins`.
4.  **Verify structure**: OBS Studio 33 and later use the new plugin layout. Move the `.dll` out of `bin\64bit` so it sits directly in the plugin folder next to `data`:
    ```text
    <plugins folder>\
    └── bilibili-stream-for-obs\
        ├── bilibili-stream-for-obs.dll
        └── data\
            └── locale\
                └── (language .ini files)
    ```
    On OBS Studio 30–32, keep the legacy `bin\64bit` layout:
    ```text
    <plugins folder>\
    └── bilibili-stream-for-obs\
        ├── bin\
        │   └── 64bit\
        │       └── bilibili-stream-for-obs.dll
        └── data\
            └── locale\
                └── (language .ini files)
    ```
    > The legacy layout still loads in OBS Studio 33 but is flagged as "Legacy" by the Plugin Manager. It is no longer supported starting with OBS Studio 34, so migrating to the new layout is recommended.
5.  **Launch OBS**: Restart OBS Studio, the plugin will load automatically.
6.  **Confirm installation**: Open **Tools → Plugin Manager** in OBS and check that `BILIBILI Stream for OBS` is loaded.

### macOS

1.  **Download plugin**: Download the latest `bilibili-stream-for-obs-*-macos-universal.pkg` from the [Releases page](https://github.com/Zarosmm/obs-bilibili-stream/releases).
2.  **Install plugin**: Double-click the `.pkg` file and follow the installation wizard.
3.  **Launch OBS**: Restart OBS Studio, the plugin will load automatically.

The plugin will be installed to `/Library/Application Support/obs-studio/plugins/`.

### Ubuntu/Debian

1.  **Download plugin**: Download the latest `.deb` package (e.g., `bilibili-stream-for-obs-*-x86_64-ubuntu-22.04.deb`) from the [Releases page](https://github.com/Zarosmm/obs-bilibili-stream/releases).
2.  **Install plugin**: Run the following command:
    ```bash
    sudo dpkg -i bilibili-stream-for-obs-*.deb
    ```
3.  **Fix dependencies** (if needed):
    ```bash
    sudo apt-get install -f
    ```
4.  **Launch OBS**: Restart OBS Studio, the plugin will load automatically.

The plugin will be installed to `/usr/lib/obs-plugins/` directory.

### Flatpak (OBS Studio Flatpak)

The Flatpak plugin is only compatible with the stable Flatpak build of OBS Studio and is currently provided for x86_64.

1. **Download plugin**: Download the latest `bilibili-stream-for-obs-*-flatpak-x86_64.flatpak` from the [Releases page](https://github.com/Zarosmm/obs-bilibili-stream/releases).
2. **Install plugin**: Run the following command in the download directory:
    ```bash
    flatpak install --user ./bilibili-stream-for-obs-*-flatpak-x86_64.flatpak
    ```
3. **Launch OBS**: Start the Flatpak build of OBS Studio; the plugin will load automatically:
    ```bash
    flatpak run com.obsproject.Studio
    ```

To update the plugin, download the new bundle and run:

```bash
flatpak install --user --or-update ./bilibili-stream-for-obs-*-flatpak-x86_64.flatpak
```

To uninstall the plugin, run:

```bash
flatpak uninstall --user com.obsproject.Studio.Plugin.BilibiliStream
```

This bundle cannot be used with a native OBS Studio installation from a system package. Use a `.deb` or `.rpm` package for those installations.

### Fedora/openSUSE（Unoffical）

1.  **Download plugin**： Download the latest '.rpm' packages（e.g., `obs-bilibili-stream-2.1.3-5.1.x86_64.rpm）from the [openSUSE Build Service](https://build.opensuse.org/package/show/home:whoaml/obs-bilibili-stream).
2.  **Install plugin**：Run the following command：
    ```bash
    sudo rpm -i obs-bilibili-stream-*.rpm
    ```
3.  **Launch OBS**: Restart OBS Studio, the plugin will load automatically.

The plugin will be installed to `/usr/lib/obs-plugins/` directory.


## Usage

1. **Login to Bilibili**:
    - Open OBS Studio, go to menu **Bilibili直播** → **登录** (Login).
    - Select **扫码登录** (QR Code Login) and scan with your Bilibili mobile app.
    - After successful login, the **登录** submenu shows **已登录** (Logged in). Room actions become available once the room is ready.

2. **Update Livestream Room Info**:
    - Go to **Bilibili直播** → **更新直播间信息** (Update Room Info).
    - Enter room title, select category and subcategory (e.g., "网游" → "英雄联盟").
    - Click "确认" (Confirm) to save settings.

3. **Start Streaming**:
    - Go to **Bilibili直播** → **开始直播** (Start Streaming).
    - Use the individual copy buttons for the **Server Address** and **Stream Key**. The key is hidden by default.
    - In OBS, go to **Settings** → **Stream**, set:
        - Service: Custom
        - Server: Paste RTMP address (e.g., `rtmp://live-push.bilivideo.com/live-bvc/`)
        - Stream Key: Paste the stream key (e.g., `?streamname=...`)
    - Click **开始直播** (Start Streaming) in OBS.

4. **End Streaming**:
    - Click **停止直播** (Stop Streaming) in OBS.
    - Go to **Bilibili直播** → **停止直播** to close the Bilibili livestream room.

## Troubleshooting

- **Stream fails / RTMP address unavailable**: Bilibili's streaming servers do not all support the same codecs (H.264, HEVC, etc.). If streaming fails with the address returned by the plugin, try one of the alternate servers preset by Bilibili in OBS. Keep using the stream key provided by the plugin:

  | Server | RTMP address |
  | --- | --- |
  | Global - primary | `rtmp://live-push.bilivideo.com/live-bvc/` |
  | Outside mainland China - primary | `rtmp://bdy.live-push.bilivideo.com/live-bvc/` |
  | Mainland China - backup | `rtmp://txy2.live-push.bilivideo.com/live-bvc/` |
  | Outside mainland China - backup | `rtmp://txy.live-push.bilivideo.com/live-bvc/` |

- **Privacy**: The plugin logs operation names and diagnostic codes. It removes stream credentials and redundant account fields when loading old plugin settings. The local plugin config still contains the Bilibili login cookie for session restore. Keep the plugin config, previous-version logs, and OBS profiles with saved stream keys private.

- **Logs**: If you encounter issues, check OBS log files:
  - Windows: `C:\Users\<YourUser>\AppData\Roaming\obs-studio\logs`
  - macOS: `~/Library/Logs/obs-studio/`
  - Linux: `~/.config/obs-studio/logs/`

## Requirements

- OBS Studio 30.0 or higher
- Windows / macOS / Ubuntu 22.04 or higher

## Contributing

Feel free to submit issues or pull requests to the [GitHub repository](https://github.com/Zarosmm/obs-bilibili-stream).

## Star History

[![Star History Chart](https://api.star-history.com/svg?repos=Zarosmm/obs-bilibili-stream&type=date&legend=top-left)](https://www.star-history.com/#Zarosmm/obs-bilibili-stream&type=date&legend=top-left)
