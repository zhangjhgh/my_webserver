#!/bin/bash
# test_file_upload_only.sh

SERVER_URL="http://localhost:8080/upload"
CONCURRENT=3
REQUESTS=9
UPLOAD_DIR="/home/zhangsan/~projects/my_webserver/uploads"

echo "=== 纯文件上传并发测试 ==="
echo "并发数: $CONCURRENT"
echo "总请求数: $REQUESTS"

# 检查上传目录
echo "=== 目录检查 ==="
echo "上传目录: $UPLOAD_DIR"
if [ ! -d "$UPLOAD_DIR" ]; then
    echo "❌ 错误: 上传目录不存在!"
    exit 1
else
    echo "✅ 上传目录存在"
    echo "目录权限: $(ls -ld "$UPLOAD_DIR")"
fi

# 创建测试文件
echo "创建测试文件..."
for i in $(seq 1 $REQUESTS); do
    dd if=/dev/urandom of=upload_test_$i.dat bs=100K count=5 2>/dev/null
    actual_size=$(stat -c%s "upload_test_$i.dat")
    echo "创建 upload_test_$i.dat (实际大小: $actual_size 字节)"
done

# 清理上传目录 - 详细调试
echo "=== 清理上传目录 ==="
echo "清理前文件列表:"
find "$UPLOAD_DIR" -name "*upload_test*" -type f 2>/dev/null | while read file; do
    echo "  - $file ($(stat -c%s "$file") 字节)"
done
clean_count=$(find "$UPLOAD_DIR" -name "*upload_test*" -type f 2>/dev/null | wc -l)
echo "找到 $clean_count 个待清理文件"
rm -f "$UPLOAD_DIR"/*upload_test*.dat
echo "清理完成"

# 并发上传
echo "开始并发上传..."
start_time=$(date +%s.%N)

i=1
while [ $i -le $REQUESTS ]; do
    for j in $(seq 1 $CONCURRENT); do
        if [ $i -le $REQUESTS ]; then
            file="upload_test_$i.dat"
            echo "启动上传: $file"
            curl -s -F "file=@$file" "$SERVER_URL" > "result_$i.json" 2>&1 &
            pids[$j]=$!
            i=$((i + 1))
        fi
    done
    
    # 等待这一批完成
    echo "等待当前批次完成..."
    for pid in "${pids[@]}"; do
        if [ $pid -ne 0 ]; then
            wait $pid
        fi
    done
    unset pids
    echo "当前批次完成"
    
    # 每批完成后检查文件状态
    echo "=== 批次完成检查 ==="
    find "$UPLOAD_DIR" -name "*upload_test*" -type f 2>/dev/null | while read file; do
        echo "  - 找到: $file ($(stat -c%s "$file") 字节)"
    done
done

end_time=$(date +%s.%N)
duration=$(echo "$end_time - $start_time" | bc)

echo "=== 测试完成 ==="
echo "总耗时: $duration 秒"

# 验证结果
echo "验证上传结果..."
success_count=0
for i in $(seq 1 $REQUESTS); do
    if [ -f "result_$i.json" ]; then
        if grep -q "success" "result_$i.json"; then
            success_count=$((success_count + 1))
            echo "✅ 请求 $i: 成功"
            # 显示服务器返回的具体信息
            echo "   服务器响应: $(cat "result_$i.json")"
        else
            echo "❌ 请求 $i: 失败 - $(cat result_$i.json)"
        fi
    else
        echo "❌ 请求 $i: 无结果文件"
    fi
done

echo "成功率: $success_count/$REQUESTS"

# 等待文件系统同步
echo "等待文件系统同步..."
sleep 2

# 详细检查实际上传的文件
echo "=== 详细文件检查 ==="
echo "上传目录完整列表:"
ls -la "$UPLOAD_DIR/" 2>/dev/null || echo "无法访问上传目录"

echo "使用find命令搜索:"
find "$UPLOAD_DIR" -type f 2>/dev/null | head -10
echo "..."

uploaded_files=$(find "$UPLOAD_DIR" -name "*upload_test*" -type f 2>/dev/null | wc -l)
echo "实际上传文件数: $uploaded_files"

if [ $uploaded_files -gt 0 ]; then
    echo "上传的文件详情:"
    find "$UPLOAD_DIR" -name "*upload_test*" -type f 2>/dev/null | while read file; do
        echo "  - $file ($(stat -c%s "$file") 字节, 修改时间: $(stat -c%y "$file"))"
    done
fi

# 检查文件大小匹配
echo "=== 文件大小验证 ==="
for i in $(seq 1 $REQUESTS); do
    original_size=$(stat -c%s "upload_test_$i.dat" 2>/dev/null || echo 0)
    uploaded_file=$(find "$UPLOAD_DIR" -name "*upload_test_$i.dat" -type f 2>/dev/null | head -1)
    if [ -n "$uploaded_file" ]; then
        uploaded_size=$(stat -c%s "$uploaded_file" 2>/dev/null || echo 0)
        echo "文件 $i: 原始=$original_size 字节, 上传=$uploaded_size 字节, 匹配=$([ "$original_size" -eq "$uploaded_size" ] && echo "✅" || echo "❌")"
    else
        echo "文件 $i: 原始=$original_size 字节, 上传=未找到"
    fi
done

# 清理
echo "清理测试文件..."
rm -f upload_test_*.dat result_*.json

echo "=== 测试结束 ==="