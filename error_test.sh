#!/bin/bash
# error_test.sh - 错误处理测试

SERVER_URL="http://localhost:8080/upload"

echo "=== 错误处理测试 ==="

# 测试用例数组
declare -A test_cases=(
    ["空文件"]="EMPTY"
    ["超大文件(2GB)"]="OVERSIZE" 
    ["错误Content-Type"]="WRONG_TYPE"
    ["恶意文件名"]="MALICIOUS_NAME"
    ["断点续传"]="PARTIAL_UPLOAD"
    ["并发冲突"]="CONCURRENT_CONFLICT"
)

# 1. 空文件测试
echo "1. 测试空文件上传..."
touch empty_file.dat
response=$(curl -s -F "file=@empty_file.dat" "$SERVER_URL")
echo "响应: $response"
rm -f empty_file.dat
echo ""

# 2. 超大文件测试（创建1MB文件但声称很大）
echo "2. 测试文件大小检查..."
dd if=/dev/zero of=fake_large.dat bs=1M count=1 2>/dev/null
# 模拟大文件上传 - 使用分块传输
response=$(curl -s -H "Content-Length: 2147483647" -F "file=@fake_large.dat" "$SERVER_URL" 2>&1)
echo "响应: $response"
rm -f fake_large.dat
echo ""

# 3. 错误Content-Type测试
echo "3. 测试错误Content-Type..."
echo "test data" > text_file.txt
response=$(curl -s -H "Content-Type: application/json" -F "file=@text_file.txt" "$SERVER_URL")
echo "响应: $response"
rm -f text_file.txt
echo ""

# 4. 恶意文件名测试
echo "4. 测试恶意文件名..."
dd if=/dev/zero of="malicious_../../file.dat" bs=1K count=1 2>/dev/null
response=$(curl -s -F "file=@malicious_../../file.dat" "$SERVER_URL")
echo "响应: $response"
rm -f "malicious_../../file.dat"
echo ""

# 5. 并发冲突测试
echo "5. 测试并发冲突..."
dd if=/dev/zero of=concurrent_test.dat bs=1M count=5 2>/dev/null
# 同时上传同一个文件多次
for i in {1..3}; do
    curl -s -F "file=@concurrent_test.dat" "$SERVER_URL" > "concurrent_result_$i.json" &
done
wait
# 检查结果
for i in {1..3}; do
    if [ -f "concurrent_result_$i.json" ]; then
        echo "并发请求 $i: $(cat "concurrent_result_$i.json")"
    fi
done
rm -f concurrent_test.dat concurrent_result_*.json
echo ""

echo "=== 错误处理测试完成 ==="