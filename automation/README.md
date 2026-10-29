# 科技新闻自动发布

此模块每天按北京时间 **06:00、09:00、11:00、14:00、17:00、19:00、21:00** 运行。每次从 IT之家、TechCrunch 和 The Verge 的 RSS 订阅源挑选一篇 36 小时内尚未发布的文章，用 ChatGPT 网页生成中文短文，然后通过微博网页版直接发布。没有新文章时会跳过。电脑关机期间错过的时段不会补发。

## 首次设置

在有图形桌面的终端运行：

```bash
bash automation/setup.sh
```

如果使用已安装的 `.deb`，脚本通常位于 `/usr/share/stellarsue/automation/setup.sh`；AppImage 用户可通过软件托盘菜单中的“科技新闻自动发布 → 查看启用方法”获取实际路径。需要 Python 3、`venv`、Microsoft Edge、网络连接和当前用户的 systemd 会话。请先在平常使用的 Edge 中登录 ChatGPT 和微博。设置脚本会安装 Browser Harness。请在 Edge 地址栏**手动**打开 `edge://inspect`，点左侧“远程调试”，勾选“允许对此浏览器实例进行远程调试”；如果 Edge 弹出连接确认，请点“允许”。程序会在现有 Edge 中打开两个后台标签页检查登录状态，验证通过后才启用定时器。

定时器在 `~/.config/systemd/user/stellarvoyager-news.timer`。脚本会复制到稳定的 `~/.local/share/stellarvoyager/automation/`，因此 AppImage 退出后定时任务仍能找到执行文件。运行时 Edge 和桌面会话需要保持打开；浏览器重启后可能需要再次允许调试连接。升级软件后若想使用新版任务程序，需要重新运行设置脚本。

## 运行与排查

```bash
systemctl --user list-timers stellarvoyager-news.timer
journalctl --user -u stellarvoyager-news.service -n 50 --no-pager
~/.local/share/stellarvoyager/venv/bin/python ~/.local/share/stellarvoyager/automation/news_publisher.py status
```

软件托盘菜单可查看上次状态、暂停或恢复，以及立即执行一次。运行记录在 `~/.local/state/stellarvoyager/news-state.json`，订阅源与启用状态在 `~/.config/stellarvoyager/news.json`。可编辑 `feeds` 列表以更换 RSS/Atom 来源；定时点由 systemd timer 配置。

如果点击了微博“发布”按钮却没有看到成功提示，任务会记为 `uncertain`，后续不会自动重发同一链接。请登录微博核查，再决定是否手动处理。微博网页结构、登录验证及 AI 内容声明入口可能变化；任务会在找不到对应控件时停止，不会跳过声明直接发布。

关闭自动发布：在托盘菜单取消“启用定时发布”，或运行 `systemctl --user disable --now stellarvoyager-news.timer`。前者保留定时器，但执行器会立即跳过。
