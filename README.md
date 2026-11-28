# OBS Bilibili 直播插件

这是一个为 OBS Studio 开发的插件，用于简化在 Bilibili 平台上的直播流程。插件支持扫码登录 Bilibili，更新直播间信息，并获取 RTMP 推流地址和推流码。

**[English README](README_en.md)**

## 安装方法

<div align="center">
  <img width="785" height="916" alt="image" src="https://github.com/user-attachments/assets/bf0b35cb-b7ce-49d5-b783-41cbaffefd08" />
  <p><i>OBS 官方插件安装路径说明</i></p>
</div>

### Windows

#### 方式一：安装包（推荐）

1.  **下载安装包**：从 [Releases 页面](https://github.com/Zarosmm/obs-bilibili-stream/releases) 下载最新的 `bilibili-stream-for-obs-*-windows-x64.exe`。
2.  **关闭 OBS**：安装前请先退出 OBS Studio。
3.  **运行安装包**：双击运行（需要管理员权限），选择安装方式：
    - **安装版 OBS**：安装到 `C:\ProgramData\obs-studio\plugins\bilibili-stream-for-obs`
    - **便携版 / 绿色版 OBS（portable，包括 scoop 等免安装方式）**：选择 OBS 根目录（需包含 `bin\64bit\obs64.exe`），安装到 `<OBS 根目录>\plugins\bilibili-stream-for-obs`。便携版安装适用于 OBS Studio 33 或更高版本；更早的便携版请使用下方的 zip 手动安装
4.  **启动 OBS**：重新启动 OBS Studio，插件将自动加载。
5.  **确认安装**：可在 OBS 的 **工具 → 插件管理器** 中确认 `BILIBILI Stream for OBS` 已加载。
6.  **卸载**：在 Windows 的 **设置 → 应用** 中找到 `BILIBILI Stream for OBS` 卸载；插件配置保存在 OBS 配置目录中，卸载后不会被删除。

安装包会同时写入 OBS 33+ 的新布局和 OBS 30–32 的旧布局：OBS 33+ 优先加载新布局并忽略重复的旧布局，OBS 34 起旧布局不再加载。

> 安装包当前未签名，首次运行时可能出现 SmartScreen 提示，选择「更多信息」→「仍要运行」即可。

静默安装：

```bat
bilibili-stream-for-obs-2.1.6-windows-x64.exe /VERYSILENT /SUPPRESSMSGBOXES
bilibili-stream-for-obs-2.1.6-windows-x64.exe /PORTABLE="D:\OBS" /VERYSILENT /SUPPRESSMSGBOXES
```

#### 方式二：手动安装（zip）

1.  **下载插件**：从 [Releases 页面](https://github.com/Zarosmm/obs-bilibili-stream/releases) 下载最新的 `bilibili-stream-for-obs-*-windows-x64.zip`。
2.  **解压文件**：将压缩包解压。
3.  **确定插件目录**：
    - **安装版 OBS**：`C:\ProgramData\obs-studio\plugins`
    - **便携版 / 绿色版 OBS（portable，包括 scoop 等免安装方式）**：OBS 根目录下的 `plugins`，即与 `bin` 文件夹同级。例如 `obs64.exe` 位于 `X:\xxx\obs-studio\bin\64bit\`，插件目录就是 `X:\xxx\obs-studio\plugins`。
      > 便携版**不会**读取 `C:\ProgramData\obs-studio\plugins` 中的插件，放在那里不会生效。
4.  **校验结构**：OBS Studio 33 起使用新的插件结构，把解压包中的 `.dll` 从 `bin\64bit` 移到插件文件夹内，与 `data` 同级：
    ```text
    <插件目录>\
    └── bilibili-stream-for-obs\
        ├── bilibili-stream-for-obs.dll
        └── data\
            └── locale\
                └── (相关的 .ini 语言文件)
    ```
    如果仍使用 OBS Studio 30–32，请保持旧的 `bin\64bit` 结构：
    ```text
    <插件目录>\
    └── bilibili-stream-for-obs\
        ├── bin\
        │   └── 64bit\
        │       └── bilibili-stream-for-obs.dll
        └── data\
            └── locale\
                └── (相关的 .ini 语言文件)
    ```
    > 旧结构在 OBS Studio 33 中依然能加载，但会被插件管理器标记为 “Legacy”，从 OBS Studio 34 起不再支持，建议升级到新结构。
5.  **启动 OBS**：重新启动 OBS Studio，插件将自动加载。
6.  **确认安装**：可在 OBS 的 **工具 → 插件管理器** 中确认 `BILIBILI Stream for OBS` 已加载。

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

### Flatpak（OBS Studio Flatpak）

Flatpak 插件仅适用于通过 Flatpak 安装的 OBS Studio stable 版本，目前提供 x86_64 架构。

1. **下载插件**：从 [Releases 页面](https://github.com/Zarosmm/obs-bilibili-stream/releases) 下载最新的 `bilibili-stream-for-obs-*-flatpak-x86_64.flatpak`。
2. **安装插件**：在下载目录运行：
    ```bash
    flatpak install --user ./bilibili-stream-for-obs-*-flatpak-x86_64.flatpak
    ```
3. **启动 OBS**：运行 Flatpak 版 OBS Studio，插件将自动加载：
    ```bash
    flatpak run com.obsproject.Studio
    ```

更新插件时，下载新的 bundle 并运行：

```bash
flatpak install --user --or-update ./bilibili-stream-for-obs-*-flatpak-x86_64.flatpak
```

卸载插件：

```bash
flatpak uninstall --user com.obsproject.Studio.Plugin.BilibiliStream
```

该 bundle 不适用于通过系统软件包安装的原生 OBS Studio；此类安装请使用 `.deb` 或 `.rpm` 包。

### Fedora/openSUSE（非官方）

1.  **下载插件**：从 [openSUSE Build Service](https://build.opensuse.org/package/show/home:whoaml/obs-bilibili-stream) 下载最新的 `.rpm` 包（例如 `obs-bilibili-stream-2.1.3-5.1.x86_64.rpm）。
2.  **安装插件**：运行以下命令：
    ```bash
    sudo rpm -i obs-bilibili-stream-*.rpm
    ```

3.  **启动 OBS**：重新启动 OBS Studio，插件将自动加载。

插件将安装到 `/usr/lib/obs-plugins/` 目录。


## 使用方法

1. **登录 Bilibili**：
    - 打开 OBS Studio，导航到菜单栏的 **Bilibili直播** → **登录**。
    - 选择 **扫码登录**，用手机 Bilibili App 扫码并确认。
    - 登录成功后，**登录** 子菜单将显示 **已登录**。直播间就绪后即可使用开播、打开直播间及更新直播间信息。

2. **更新直播间信息**：
    - 导航到 **Bilibili直播** → **更新直播间信息**。
    - 输入直播间标题，选择直播分区和子分区（例如"网游" → "英雄联盟"）。
    - 点击"确认"保存设置。

3. **开始直播**：
    - 导航到 **Bilibili直播** → **开始直播**。
    - 在弹窗中分别复制 **服务器地址** 和 **推流码**。推流码默认隐藏，可按需显示。
    - 在 OBS 中，打开 **设置** → **推流**，设置：
        - 服务：自定义
        - 服务器：粘贴 RTMP 地址（例如 `rtmp://live-push.bilivideo.com/live-bvc/`）
        - 流密钥：粘贴推流码（例如 `?streamname=...`）
    - 点击 OBS 界面右下角的 **开始直播** 按钮。

4. **结束直播**：
    - 在 OBS 界面右下角点击 **停止直播**。
    - 返回到 **Bilibili直播** → **停止直播**，以关闭 Bilibili 直播间。

## 注意事项

- **推流失败 / RTMP 地址不可用**：B 站的不同推流服务器对编码格式的支持并不一致（H.264、HEVC 等），如果使用插件返回的地址推流失败，可以改用 B 站官方预设的其他服务器地址，推流码仍使用插件提供的内容：

  | 服务器 | RTMP 推流地址 |
  | --- | --- |
  | 全球 - 主要 | `rtmp://live-push.bilivideo.com/live-bvc/` |
  | 非中国大陆地区 - 主要 | `rtmp://bdy.live-push.bilivideo.com/live-bvc/` |
  | 中国大陆地区 - 备用 | `rtmp://txy2.live-push.bilivideo.com/live-bvc/` |
  | 非中国大陆地区 - 备用 | `rtmp://txy.live-push.bilivideo.com/live-bvc/` |

- **隐私**：插件只记录操作名、状态码等诊断信息。新版加载旧配置时会移除插件配置里的推流地址、推流码和冗余账号字段。用于恢复登录的 Bilibili Cookie 仍保存在本机插件配置中。分享诊断资料时，请避开插件配置、旧版日志和含推流码的 OBS 配置。

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
