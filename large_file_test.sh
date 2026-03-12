#!/bin/bash
# large_file_test_fixed.sh - 最终修复版

SERVER_URL="http://localhost:8080/upload"
UPLOAD_DIR="/home/zhangsan/~projects/my_webserver/uploads"

echo "=== 大文件上传测试 ==="

# 大文件测试配置
declare -A large_files=(
    ["10MB"]=10
    ["50MB"]=50
    ["100MB"]=100
    ["500MB"]=500
)

# 记录测试开始时间
test_start_time=$(date +%s)

for size_name in "${!large_files[@]}"; do
    file_size_mb="${large_files[$size_name]}"
    test_file="large_test_${size_name}.dat"
    
    echo "📁 测试 $size_name 文件上传..."
    
    # 创建大文件
    echo "创建 $size_name 测试文件..."
    dd if=/dev/zero of="$test_file" bs=1M count=$file_size_mb 2>/dev/null
    actual_size=$(stat -c%s "$test_file")
    echo "文件大小: $((actual_size / 1024 / 1024))MB ($actual_size 字节)"
    
    # 清理同名的旧测试文件
    rm -f "$UPLOAD_DIR"/*large_test_${size_name}*.dat
    
    # 使用高精度时间戳
    upload_start_time=$(date +%s.%N)
    
    # 上传测试
    response=$(curl -s -F "file=@$test_file" "$SERVER_URL")
    upload_end_time=$(date +%s.%N)
    
    # 修复除零错误：使用bc计算浮点数持续时间
    duration=$(echo "$upload_end_time - $upload_start_time" | bc)
    
    # 分析结果
    if echo "$response" | grep -q "success"; then
        echo "✅ 上传成功"
        
        # 修复：安全计算上传速度（避免除零）
        if [ $(echo "$duration > 0.001" | bc) -eq 1 ]; then
            upload_speed=$(echo "scale=2; $file_size_mb / $duration" | bc)
            echo "  耗时: $duration 秒"
            echo "  速度: $upload_speed MB/s"
        else
            echo "  耗时: <0.001 秒"
            echo "  速度: 极快"
        fi
        
        # 查找上传的文件
        uploaded_file=$(find "$UPLOAD_DIR" -name "*large_test_${size_name}*.dat" -type f -newermt "@$test_start_time" | head -1)
        
        if [ -n "$uploaded_file" ] && [ -f "$uploaded_file" ]; then
            uploaded_size=$(stat -c%s "$uploaded_file" 2>/dev/null || echo 0)
            echo "  上传文件: $(basename "$uploaded_file")"
            echo "  上传大小: $((uploaded_size / 1024 / 1024))MB ($uploaded_size 字节)"
            
            if [ "$actual_size" -eq "$uploaded_size" ]; then
                echo "✅ 文件完整性验证通过"
            else
                echo "❌ 文件大小不匹配"
            fi
        else
            echo "❌ 上传文件未找到"
        fi
    else
        echo "❌ 上传失败: $response"
    fi
    
    echo ""
    
    # 清理测试文件
    rm -f "$test_file"
    sleep 1
done

echo "=== 大文件测试完成 ==="