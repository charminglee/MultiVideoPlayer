#!/usr/bin/env bash
# 把 Qt 运行库、mpv 及其全部依赖 DLL 打包到 exe 所在目录，
# 让应用可以脱离 MSYS2 环境直接双击运行。
# 用法: bundle.sh <exe路径> <mpv构建目录>
set -e

EXE="$1"
MPV_BUILD="$2"
OUT="$(dirname "$EXE")"

# 1) libmpv 本体
cp -u "$MPV_BUILD/libmpv-2.dll" "$OUT/"

# 2) exe + libmpv 的完整依赖闭包（Qt、libc++、ffmpeg 等，递归解析）
for d in $(ldd "$EXE" "$MPV_BUILD/libmpv-2.dll" | grep clang64 | cut -d" " -f3 | sort -u); do
    cp -u "$d" "$OUT/"
done

# 3) Qt 插件（平台插件 platforms/qwindows.dll 必需，但它是运行时加载，
#    ldd 看不到，必须由 windeployqt 按目录结构拷贝）
windeployqt6 --release --no-translations --no-system-d3d-compiler --no-opengl-sw "$EXE"

echo "bundle: $(ls "$OUT"/*.dll 2>/dev/null | wc -l) 个 DLL + $(find "$OUT" -mindepth 2 -name '*.dll' | wc -l) 个插件 DLL 已就位"
