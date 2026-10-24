# StellarVoyager

**让你的电脑，成为一艘穿行星海的飞船。**

An immersive Linux system monitor that brings real-time system telemetry into an interactive 3D universe, featuring a black hole, vibrant nebulae, and an orbiting spacecraft.

StellarVoyager 是一款使用 **C++17 · Qt Widgets · OpenGL** 构建的 Linux 桌面系统监控应用。它将 CPU、内存、磁盘、网络和 GPU 数据融入可旋转的三维宇宙：航天飞机沿固定航线巡航，黑洞与环绕星云构成背景，本机运行状态以飞行器遥测的方式呈现。

![StellarVoyager 场景预览，使用测试数据](docs/images/scene-preview.png)

*截图使用演示数据，GPU 不可用时显示 N/A。当前界面仍保留早期 Stellarsue 标识；项目正式名称为 StellarVoyager。*

## 特性

- **实时遥测**：CPU 占用与逻辑核心数、内存、根分区磁盘、网络上下行速率、运行时间，以及驱动支持时的 GPU 负载、显存和温度。
- **交互式三维宇宙**：鼠标旋转、缩放和复位；14 组远景星云分布于四周及上下方，不同朝向呈现不同景色。
- **黑洞视觉效果**：物理启发的吸积盘、光子环、透镜光带和明暗变化，与飞船保持前后遮挡关系。
- **真实三维飞行器素材**：基于 NASA/Johnson Space Center 的 Space Shuttle (D) 模型，包含 37,004 个三角形和 11 张表面贴图。
- **固定航线巡航**：每 48 秒完成一圈，机头跟随航线切线，配有细线轨迹和尾迹；隐藏窗口时暂停飞行计时。
- **桌面融合**：无边框、半透明、默认置顶，仅在边缘 10 个逻辑像素内羽化，保留内部场景清晰度。
- **紧凑悬停详情**：278px 宽的半透明圆角遥测卡，保留完整读数；移出、拖动或缩放时自动收起。
- **共享字体安装**：可选离线安装 Times New Roman 四种字形，供当前用户的其他桌面程序使用；中文采用衬线字体回退。
- **本地运行**：读取 Linux 系统接口，无需监控服务端。模型、贴图及可选字体安装包内置于可执行文件。

## 环境要求

- Linux 桌面环境，支持 OpenGL 3.3 Core；透明效果需要桌面合成器。
- C++17 编译器、CMake 3.16 或更新版本。
- **Qt 6.9 或更新的 Qt 6 版本**，包含 Widgets、OpenGL、OpenGLWidgets 组件和 JPEG 图像支持。
- 已验证环境：Ubuntu 24.04 / X11 / Qt 6.9.2。Wayland 原生会话的窗口定位、置顶和托盘行为尚未验证。

当前代码使用了 Qt 6.9 引入的 [`QImage::flipped()`](https://doc.qt.io/qt-6/qimage.html#flipped)，不能只安装较旧的发行版 Qt 包。可参考 [Qt 官方安装指南](https://doc.qt.io/qt-6/get-and-install-qt.html) 准备匹配的 Qt 6 SDK。

Ubuntu 上的基础工具、字体回退及可选字体解包依赖：

```bash
sudo apt install build-essential cmake libgl1-mesa-dev \
    fonts-liberation fonts-noto-cjk fontconfig cabextract desktop-file-utils
```

以上命令不安装 Qt 6.9 SDK。`cabextract` 可用 `bsdtar` 替代；不使用字体安装功能时可不安装解包工具。

## 编译与运行

```bash
git clone https://github.com/Stellarsue/StellarVoyager.git
cd StellarVoyager

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/system-monitor
```

如果 CMake 找不到正确的 Qt，配置时增加 `-DCMAKE_PREFIX_PATH=/path/to/Qt/6.9.2/gcc_64`，将路径替换为实际 SDK 目录。运行环境也需要匹配的 Qt 动态库与平台插件；本项目当前不是独立的 AppImage 安装包。

为兼容已有桌面安装，暂时保留 `system-monitor` 可执行文件名、`linux-system-monitor.desktop` 桌面文件名，以及 `STELLARSUE_` 开头的构建选项和资源路径。

## 安装到当前用户

```bash
cmake --install build --prefix "$HOME/.local"
update-desktop-database "$HOME/.local/share/applications"
```

确保 `~/.local/bin` 已加入桌面会话的 `PATH`，否则需将安装后的桌面文件中 `Exec` 和 `TryExec` 改为可执行文件的绝对路径。桌面应用列表当前显示旧名称 **Stellarsue**。

需要系统级安装时可运行 `sudo cmake --install build`；应用日常运行和用户字体安装不需要 root。

### 开机自动启动

先完成用户级安装，再复制桌面启动项：

```bash
mkdir -p "$HOME/.config/autostart"
cp "$HOME/.local/share/applications/linux-system-monitor.desktop" \
   "$HOME/.config/autostart/linux-system-monitor.desktop"
```

如果应用不在桌面会话的 `PATH` 中，同样需要修改 autostart 文件的 `Exec` 和 `TryExec`。

## 操作方式

| 操作 | 效果 |
| --- | --- |
| 拖动星空 | 旋转三维视角，探索不同方向的星云 |
| 滚动鼠标滚轮 | 缩放场景 |
| 双击星空 | 复位视角与缩放 |
| 悬停星空 | 查看完整系统遥测 |
| 拖动顶部控制条 | 移动窗口 |
| 图钉 / 刷新按钮 | 切换置顶 / 立即采样 |
| 关闭按钮 | 有托盘时隐藏窗口；无托盘时退出 |
| 托盘菜单 | 显示窗口、安装共享字体或退出 |

默认窗口约为 480 × 470；水平旋转连续 360°，俯仰范围约为 ±69°。

## 字体

英文和数字优先使用 **Times New Roman**，缺失时回退到 **Liberation Serif**；中文使用 **Noto Serif CJK SC**。

默认构建携带完整、未经修改的微软历史 Core Fonts for the Web 原始安装包及 EULA，**不是从 Windows 系统复制的新版字体，也不单独重新分发 TTF**。

- 首次启动且字体缺失时显示许可；同意后离线解压常规、粗体、斜体、粗斜体四种字形。
- 可从托盘菜单再次安装。暂不接受许可不会影响应用使用回退字体。
- 仅解压原始包，不执行 Windows EXE；校验 SHA-256、保留许可并刷新字体缓存。
- 默认安装到 `~/.local/share/fonts/stellarsue-times`；设置 `XDG_DATA_HOME` 时使用其下的 `fonts/stellarsue-times`。
- 不修改其他程序的默认字体。其他桌面程序重启后可选择使用；沙箱应用能否读取字体取决于权限。

阅读并接受 [原始许可](assets/corefonts/EULA.html) 后，可通过命令行安装：

```bash
./build/system-monitor --install-fonts --accept-font-license
```

不携带此字体包的构建方式：

```bash
cmake -S . -B build -DSTELLARSUE_BUNDLE_TIMES=OFF
cmake --build build -j2
```

**重要：该字体许可禁止为营利目的分发，包括随收费产品分发。** 关闭字体包不代表其他代码或素材自动获得商业授权。完整来源和校验值见 [字体来源说明](assets/corefonts/SOURCE.md)。

## 数据来源与边界

- CPU、内存、网络和运行时间来自 `/proc`；根分区容量通过 Qt `QStorageInfo` 获取。
- GPU 优先查询 `nvidia-smi`，否则尝试 DRM/sysfs。具体指标取决于驱动暴露的接口；不可用的负载显示 N/A，不伪造数据。
- 网络速率是非 `lo` 网卡的累计字节差分，不是网络测速。
- 黑洞渲染是**物理启发的视觉近似**，不是广义相对论光线追踪；飞船巡航是艺术化固定路径，不是航天动力学模拟。
- 支持 Mesa 软件渲染，但相较 GPU 硬件渲染可能消耗更多 CPU。建议启用硬件加速。

## 测试与开发

```bash
cmake -S . -B build -DSTELLARSUE_BUILD_SMOKE_TESTS=ON
cmake --build build -j2

# 需要可用的 X11 桌面与 OpenGL 上下文，不能使用 offscreen 平台
QT_QPA_PLATFORM=xcb ./build/stellarsue-smoke build/previews

# 隔离的临时用户目录，不修改真实用户字体；需启用内置字体包
QT_QPA_PLATFORM=offscreen ./build/stellarsue-font-test
```

测试覆盖模型及贴图加载、固定航线连续性、相机旋转、星云分布与裁剪、隐藏/恢复计时、10px 边缘透明度、悬停卡片、遥测映射、OpenGL 资源重建、字体回退、许可拒绝、离线安装及已有文件保护。

主要代码位置：

- `systeminfo.*`：本机系统数据采集。
- `monitorwidget.*`：主窗口、控制条和系统托盘。
- `starmapwidget.*`、`blackholewidget.*`：宇宙场景、黑洞和遥测绘制。
- `nebulaenvironment.*`、`flightpath.*`、`spacecraftmesh.*`：星云分布、巡航路径和模型加载。
- `telemetrytooltip.*`、`themefonts.h`、`fontbundle.*`：主题提示卡、字体选择和安装。
- `assets/nasa-shuttle/`：原始模型、转换后的几何与贴图。

一般编译无需 Python。仅在重新生成航天飞机模型时，才需要 Python、NumPy 和 `DracoPy==2.2.0`，然后在项目根目录运行 `python tools/prepare_shuttle.py`。该脚本会重新生成模型及贴图，请先保存自己的素材改动。

## 许可与致谢

项目源代码目前尚未指定开源许可证；公开仓库本身不等于授予任意再分发或商业使用权限。如需相关授权，请联系维护者。

- **NASA/Johnson Space Center**：[Space Shuttle (D)](https://science.nasa.gov/3d-resources/space-shuttle-d/) 模型。遵循 [NASA 媒体与品牌使用指南](https://www.nasa.gov/nasa-brand-center/images-and-media/)，不代表 NASA 背书。转换与装配说明见 [素材来源](assets/nasa-shuttle/SOURCE.md)。
- **Microsoft Core Fonts for the Web**：原始完整安装包及 [EULA](assets/corefonts/EULA.html)，另见 [来源和分发限制](assets/corefonts/SOURCE.md)。
- **Qt**：应用框架与 OpenGL 集成；使用和分发时须遵守所采用 Qt 版本的许可。

早期 Kenney 模型与飞行器概念图保留在素材目录，当前不嵌入应用。Kenney 素材附带独立的 [CC0 许可](assets/kenney-space-kit/License.txt)。
