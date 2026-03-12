#!/bin/bash

# 获取脚本所在目录
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"

# 构建目录
BUILD_DIR="$PROJECT_DIR/build"

# 可执行文件
EXECUTABLE="$PROJECT_DIR/bin/webserver"

# 命令行参数（传递给 webserver）
ARGS="$@"

# 创建构建目录（如果不存在）
mkdir -p "$BUILD_DIR"

# 进入构建目录
cd "$BUILD_DIR"

# 生成 Makefile（如果第一次运行或 CMakeLists.txt 有修改）
cmake ..

# 增量编译（只编译修改过的文件）
cmake --build . -- -j4

# 检查可执行文件是否存在
if [ ! -f "$EXECUTABLE" ]; then
    echo "错误：可执行文件 $EXECUTABLE 不存在"
    exit 1
fi

# 杀掉已经在运行的 webserver 实例（忽略错误）
pkill -f "$EXECUTABLE" 2>/dev/null || true

# 等待一秒，确保旧实例退出
sleep 1

# 运行最新程序
echo "运行程序 $EXECUTABLE $ARGS ..."
"$EXECUTABLE" $ARGS