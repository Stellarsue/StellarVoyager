#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
data_dir=${XDG_DATA_HOME:-"$HOME/.local/share"}/stellarvoyager
python_bin=${PYTHON:-python3}
installed_dir="$data_dir/automation"

# AppImage mounts are temporary; systemd needs a stable copy of the worker.
mkdir -p "$installed_dir"
if [ "$script_dir" != "$installed_dir" ]; then
    install -m 644 "$script_dir/news_publisher.py" "$installed_dir/news_publisher.py"
    install -m 644 "$script_dir/requirements.txt" "$installed_dir/requirements.txt"
fi
script_dir="$installed_dir"

"$python_bin" -m venv "$data_dir/venv"
"$data_dir/venv/bin/python" -m pip install -r "$script_dir/requirements.txt"
if ! command -v microsoft-edge >/dev/null 2>&1; then
    echo '请先安装 Microsoft Edge，并在其中登录 ChatGPT 和微博。' >&2
    exit 1
fi

edge_data_dir=${XDG_CONFIG_HOME:-"$HOME/.config"}/microsoft-edge
if [ ! -f "$edge_data_dir/DevToolsActivePort" ]; then
    echo '请在平常使用的 Edge 地址栏手动打开 edge://inspect。'
    echo '点左侧“远程调试”，勾选“允许对此浏览器实例进行远程调试”。'
    for attempt in $(seq 1 120); do
        [ -f "$edge_data_dir/DevToolsActivePort" ] && break
        sleep 1
    done
fi
if [ ! -f "$edge_data_dir/DevToolsActivePort" ]; then
    echo 'Edge 尚未允许远程调试，未启用定时发布。' >&2
    exit 1
fi
echo '正在连接当前 Edge；如果出现远程调试连接请求，请点击“允许”。'
"$data_dir/venv/bin/python" "$script_dir/news_publisher.py" check-browser

"$data_dir/venv/bin/python" "$script_dir/news_publisher.py" install-timer
"$data_dir/venv/bin/python" "$script_dir/news_publisher.py" status
