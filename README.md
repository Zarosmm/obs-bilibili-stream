# OBS Bilibili 直播插件

这是一个为 OBS Studio 开发的插件，用于简化在 Bilibili 平台上的直播流程。插件支持扫码登录 Bilibili，更新直播间信息，并获取 RTMP 推流地址和推流码。

**[English README](README_en.md)**

## 安装方法

### Windows

1.  **下载插件**：从 [Releases 页面](https://github.com/Zarosmm/obs-bilibili-stream/releases) 下载最新的 `bilibili-stream-for-obs-*-windows-x64.zip`。
2.  **解压文件**：将压缩包解压。
3.  **放置目录**：将解压后的文件夹移动至以下路径：
    `C:\ProgramData\obs-studio\plugins`
4.  **校验结构**：请确保您的目录结构严格遵守以下格式：
    ```text
    C:\ProgramData\obs-studio\plugins\
    └── bilibili-stream-for-obs\
        ├── bin\
        │   └── 64bit\
        │       └── bilibili-stream-for-obs.dll
        └── data\
            └── locale\
                └── (相关的 .ini 语言文件)
    ```
5.  **启动 OBS**：重新启动 OBS Studio，插件将自动加载。

<div align="center">
  <img width="785" height="916" alt="image" src="https://github.com/user-attachments/assets/bf0b35cb-b7ce-49d5-b783-41cbaffefd08" />
  <p><i>OBS 官方插件安装路径说明</i></p>
</div>

### macOS

1.  **下载插件**：从 [Releases 页面](https://github.com/Zarosmm/obs-bilibili-stream/releases) 下载最新的 `bilibili-stream-for-obs-*-macos-universal.pkg`。
2.  **安装插件**：双击 `.pkg` 文件，按提示完成安装。
3.  **启动 OBS**：重新启动 OBS Studio，插件将自动加载。

插件将安装到 `/Library/Application Support/obs-studio/plugins/` 目录。

### Ubuntu/Debian

1.  **下载插件**：从 [Releases 页面](https://github.com/Zarosmm/obs-bilibili-stream/releases) 下载最新的 `.deb` 包（例如 `bilibili-stream-for-obs-*-x86_64-ubuntu-22.04.deb`）。
2.  **安装插件**：运行以下命令：
    ```bash
    sudo dpkg -i bilibili-stream-for-obs-*.deb
    ```
3.  **修复依赖**（如需要）：
    ```bash
    sudo apt-get install -f
    ```
4.  **启动 OBS**：重新启动 OBS Studio，插件将自动加载。

插件将安装到 `/usr/lib/obs-plugins/` 目录。

## 使用方法

1. **登录 Bilibili**：
    - 打开 OBS Studio，导航到菜单栏的 **Bilibili直播** → **登录**。
    - 选择 **扫码登录**（使用手机扫描二维码）。
    - 登录成功后，菜单中的"登录状态"将显示为"已登录"。

2. **更新直播间信息**：
    - 导航到 **Bilibili直播** → **更新直播间信息**。
    - 输入直播间标题，选择直播分区和子分区（例如"网游" → "英雄联盟"）。
    - 点击"确认"保存设置。

3. **开始直播**：
    - 导航到 **Bilibili直播** → **开始直播**。
    - 弹出提示框将显示 **RTMP 地址** 和 **推流码**，复制这些信息。
    - 在 OBS 中，打开 **设置** → **输出** → **流**，设置：
        - 服务：自定义
        - 服务器：粘贴 RTMP 地址（例如 `rtmp://live-push.bilivideo.com/live-bvc/`）
        - 流密钥：粘贴推流码（例如 `?streamname=...`）
    - 点击 OBS 界面右下角的 **开始直播** 按钮。

4. **结束直播**：
    - 在 OBS 界面右下角点击 **停止直播**。
    - 返回到 **Bilibili直播** → **停止直播**，以关闭 Bilibili 直播间。

## 注意事项

- **日志查看**：如果遇到问题，检查 OBS 日志文件：
  - Windows: `C:\Users\<YourUser>\AppData\Roaming\obs-studio\logs`
  - macOS: `~/Library/Logs/obs-studio/`
  - Linux: `~/.config/obs-studio/logs/`

## 依赖

- OBS Studio 30.0 或更高版本
- Windows / macOS / Ubuntu 22.04 或更高版本

## 贡献

欢迎提交问题或拉取请求至 [GitHub 仓库](https://github.com/Zarosmm/obs-bilibili-stream)。

## Star History

[![Star History Chart](https://api.star-history.com/svg?repos=Zarosmm/obs-bilibili-stream&type=date&legend=top-left)](https://www.star-history.com/#Zarosmm/obs-bilibili-stream&type=date&legend=top-left)
