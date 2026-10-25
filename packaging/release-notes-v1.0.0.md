# StellarVoyager v1.0.0

让你的电脑，成为一艘穿行星海的飞船。

首个 Linux x86_64 发行版，将本机 CPU、内存、磁盘、网络和 GPU 数据融入可交互的三维宇宙。

## 下载哪个文件？

- **`StellarVoyager-1.0.0-x86_64.AppImage`**：推荐直接体验，无需安装 Qt。下载后赋予执行权限并运行。
- **`StellarVoyager-1.0.0-linux-amd64.deb`**：Ubuntu 22.04 / 24.04 安装包，自动安装依赖并加入应用菜单。
- **`SHA256SUMS`**：下载校验值。
- **`build-manifest.json`**：构建版本、来源与文件信息。
- **`StellarVoyager-1.0.0-runtime-sources.tar.xz`**：第三方运行库对应源码，仅供审计/重新构建，普通用户无需下载。

```bash
chmod +x StellarVoyager-1.0.0-x86_64.AppImage
./StellarVoyager-1.0.0-x86_64.AppImage
```

FUSE 受限时使用：

```bash
./StellarVoyager-1.0.0-x86_64.AppImage --appimage-extract-and-run
```

Ubuntu 安装：

```bash
sudo apt install ./StellarVoyager-1.0.0-linux-amd64.deb
```

之后从应用菜单启动 **StellarVoyager**，也可以运行 `system-monitor`。

## 这一版包含

- 可旋转、缩放的三维黑洞、星空与环绕分布的彩色星云。
- NASA 航天飞机真实网格与贴图，沿固定航线巡航。
- 本机实时遥测与紧凑悬停详情；GPU 不可用时显示 N/A。
- 无边框透明窗口与 10px 边缘羽化，主题名称统一为 StellarVoyager。
- 英文衬线与中文回退字体；可选离线安装 Times New Roman，需明确接受原始许可。

## 兼容性与验证

- Linux **x86_64 / amd64**，glibc **2.35+**，OpenGL **3.3+**，X11 或 XWayland。
- Ubuntu 22.04 / GCC 11 / Qt 6.2.4 构建；AppImage 自带 Qt，不附带显卡驱动。
- 已在干净 Ubuntu 22.04、24.04 容器中验证 AppImage 独立启动及 `.deb` 安装/启动，使用 Xvfb + Mesa 软件渲染；另在 Ubuntu 24.04 X11 桌面启动验证。
- 三维场景交互、飞船资源、星云分布、透明边缘、悬停详情和隔离字体安装测试通过。
- 暂不提供 ARM64；没有覆盖所有显卡、发行版或桌面合成器。透明效果需合成器支持，Wayland 下定位/置顶可能有限制。
- 精简系统可能需要安装 `libopengl0 libgl1 libegl1 libfontconfig1 libharfbuzz0b`。日常运行不需要 root。

黑洞和巡航是物理启发的艺术化视觉效果，不是广义相对论或航天动力学仿真。
原始微软字体安装包禁止为营利目的分发；其他组件遵循各自许可。Qt/LGPL 替换说明及素材来源见
[第三方说明](https://github.com/Stellarsue/StellarVoyager/blob/v1.0.0/packaging/THIRD_PARTY.md)。
