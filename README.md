# OBS Bilibili 直播插件

这是一个为 OBS Studio 开发的插件，用于简化在 Bilibili 平台上的直播流程。插件支持通过手动输入 Cookie 或扫码登录 Bilibili，更新直播间信息，并获取 RTMP 推流地址和推流码。

## 安装方法

1. 从 [Releases 页面](https://github.com/Zarosmm/obs-bilibili-stream/releases) 下载最新的插件压缩包。
2. 解压压缩包到 OBS Studio 的插件目录：
    - Windows: `C:\ProgramData\obs-studio\plugins`
    - 确保解压后的文件夹结构为：`C:\ProgramData\obs-studio\plugins\bilibili-stream-for-obs`
3. 启动 OBS Studio，插件将自动加载。

## 使用方法

1. **登录 Bilibili**：
    - 打开 OBS Studio，导航到菜单栏的 **工具** → **Bilibili直播** → **登录**。
    - 选择 **手动登录**（输入 Bilibili Cookie）或 **扫码登录**（使用手机扫描二维码）。
    - 登录成功后，菜单中的“登录状态”将显示为“已登录”。

2. **更新直播间信息**：
    - 导航到 **工具** → **Bilibili直播** → **更新直播间信息**。
    - 输入直播间标题，选择直播分区和子分区（例如“网游” → “英雄联盟”）。
    - 点击“确认”保存设置。

3. **开始直播**：
    - 导航到 **工具** → **Bilibili直播** → **开始直播**。
    - 弹出提示框将显示 **RTMP 地址** 和 **推流码**，复制这些信息。
    - 在 OBS 中，打开 **设置** → **输出** → **流**，设置：
        - 服务：自定义
        - 服务器：粘贴 RTMP 地址（例如 `rtmp://live-push.bilivideo.com/live-bvc/`）
        - 流密钥：粘贴推流码（例如 `?streamname=...`）
    - 点击 OBS 界面右下角的 **开始直播** 按钮。

4. **结束直播**：
    - 在 OBS 界面右下角点击 **停止直播**。
    - 返回到 **工具** → **Bilibili直播** → **停止直播**，以关闭 Bilibili 直播间。

## 注意事项

- **日志查看**：如果遇到问题，检查 OBS 日志文件（`C:\Users\<YourUser>\AppData\Roaming\obs-studio\logs`）以获取错误信息。

## 依赖

- OBS Studio 30.0 或更高版本
- Windows 操作系统

## 贡献

欢迎提交问题或拉取请求至 [GitHub 仓库](https://github.com/Zarosmm/obs-bilibili-stream)。