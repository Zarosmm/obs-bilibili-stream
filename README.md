# OBS Bilibili 直播插件

这是一个为 OBS Studio 开发的插件，用于简化在 Bilibili 平台上的直播流程。插件支持扫码登录 Bilibili，更新直播间信息，并获取 RTMP 推流地址和推流码。

## 安装方法

为了确保插件在 OBS Studio 中正确加载并兼容未来版本，请遵循 OBS 官方推荐的安装路径：

1.  **下载插件**：从 [Releases 页面](https://github.com/Zarosmm/obs-bilibili-stream/releases) 下载最新的插件压缩包。
2.  **解压文件**：将压缩包解压。
3.  **放置目录**：将解压后的文件夹移动至以下路径：
    `C:\ProgramData\obs-studio\plugins`
4.  **校验结构**：请确保您的目录结构严格遵守以下格式（插件文件夹名称需与内部 `.dll` 文件名对应）：
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


## 使用方法

1. **登录 Bilibili**：
    - 打开 OBS Studio，导航到菜单栏的 **Bilibili直播** → **登录**。
    - 选择 **扫码登录**（使用手机扫描二维码）。
    - 登录成功后，菜单中的“登录状态”将显示为“已登录”。

2. **更新直播间信息**：
    - 导航到 **Bilibili直播** → **更新直播间信息**。
    - 输入直播间标题，选择直播分区和子分区（例如“网游” → “英雄联盟”）。
    - 点击“确认”保存设置。

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

- **日志查看**：如果遇到问题，检查 OBS 日志文件（`C:\Users\<YourUser>\AppData\Roaming\obs-studio\logs`）以获取错误信息。

## 依赖

- OBS Studio 30.0 或更高版本
- Windows 操作系统

## 贡献

欢迎提交问题或拉取请求至 [GitHub 仓库](https://github.com/Zarosmm/obs-bilibili-stream)。

## Star History

[![Star History Chart](https://api.star-history.com/svg?repos=Zarosmm/obs-bilibili-stream&type=date&legend=top-left)](https://www.star-history.com/#Zarosmm/obs-bilibili-stream&type=date&legend=top-left)
