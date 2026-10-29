#!/usr/bin/env bash
set -u

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
data_dir=${XDG_DATA_HOME:-"$HOME/.local/share"}/stellarvoyager
python_bin="$data_dir/venv/bin/python"
worker="$script_dir/news_publisher.py"

echo 'StellarVoyager · 手动发布科技新闻'
echo

if [ ! -x "$python_bin" ] || [ ! -f "$worker" ]; then
    echo '发布程序尚未安装，请先运行 automation/setup.sh。' >&2
    result=1
else
    "$python_bin" "$worker" run --force
    result=$?
fi

echo
if [ "$result" -eq 0 ]; then
    echo '任务已完成。'
else
    echo '任务执行失败，请查看上面的原因。' >&2
fi
read -r -p '按回车键关闭窗口…' _
exit "$result"
