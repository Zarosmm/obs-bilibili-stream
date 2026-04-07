# OBS Bilibili Streaming Plugin

A plugin for OBS Studio that simplifies the streaming process on Bilibili platform. Supports QR code login, updating livestream room info, and obtaining RTMP streaming address and key.

## Installation

### Windows

1.  **Download plugin**: Download the latest `bilibili-stream-for-obs-*-windows-x64.zip` from the [Releases page](https://github.com/Zarosmm/obs-bilibili-stream/releases).
2.  **Extract files**: Unzip the downloaded archive.
3.  **Move to plugins folder**: Copy the extracted folder to:
    `C:\ProgramData\obs-studio\plugins`
4.  **Verify structure**: Ensure the directory structure follows this format:
    ```text
    C:\ProgramData\obs-studio\plugins\
    └── bilibili-stream-for-obs\
        ├── bin\
        │   └── 64bit\
        │       └── bilibili-stream-for-obs.dll
        └── data\
            └── locale\
                └── (language .ini files)
    ```
5.  **Launch OBS**: Restart OBS Studio, the plugin will load automatically.

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

## Usage

1. **Login to Bilibili**:
    - Open OBS Studio, go to menu **Bilibili直播** → **登录** (Login).
    - Select **扫码登录** (QR Code Login) and scan with your Bilibili mobile app.
    - After successful login, the "登录状态" (Login Status) will show "已登录" (Logged in).

2. **Update Livestream Room Info**:
    - Go to **Bilibili直播** → **更新直播间信息** (Update Room Info).
    - Enter room title, select category and subcategory (e.g., "网游" → "英雄联盟").
    - Click "确认" (Confirm) to save settings.

3. **Start Streaming**:
    - Go to **Bilibili直播** → **开始直播** (Start Streaming).
    - Copy the **RTMP Address** and **Stream Key** from the popup dialog.
    - In OBS, go to **Settings** → **Output** → **Stream**, set:
        - Service: Custom
        - Server: Paste RTMP address (e.g., `rtmp://live-push.bilivideo.com/live-bvc/`)
        - Stream Key: Paste stream key (e.g., `?streamname=...`)
    - Click **开始直播** (Start Streaming) in OBS.

4. **End Streaming**:
    - Click **停止直播** (Stop Streaming) in OBS.
    - Go to **Bilibili直播** → **停止直播** to close the Bilibili livestream room.

## Troubleshooting

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
