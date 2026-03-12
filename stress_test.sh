#!/bin/bash
# stress_test.sh - 服务器压力测试

SERVER_URL="http://localhost:8080/upload"
UPLOAD_DIR="/home/zhangsan/~projects/my_webserver/uploads"

echo "=== 服务器压力测试 ==="

# 测试配置
declare -A test_cases=(
    ["低负载"]="10 50 1"
    ["中等负载"]="20 100 1" 
    ["高负载"]="50 200 1"
    ["极限负载"]="100 500 1"
)

for test_name in "${!test_cases[@]}"; do
    read CONCURRENT REQUESTS FILE_SIZE_MB <<< "${test_cases[$test_name]}"
    
    echo "🚀 测试: $test_name (并发: $CONCURRENT, 请求: $REQUESTS, 文件: ${FILE_SIZE_MB}MB)"
    
    # 创建测试文件
    echo "创建测试文件..."
    for i in $(seq 1 $REQUESTS); do
        dd if=/dev/urandom of=stress_test_$i.dat bs=1M count=$FILE_SIZE_MB 2>/dev/null
    done
    
# 清理上传目录
rm -f "$UPLOAD_DIR"/*stress_test_*.dat
    
    # 监控开始
    echo "开始监控系统资源..."
    sar -u -r -n DEV 1 100 > "system_${test_name}.log" 2>&1 &
    SAR_PID=$!
    
    # 压力测试
    start_time=$(date +%s.%N)
    
    i=1
    while [ $i -le $REQUESTS ]; do
        for j in $(seq 1 $CONCURRENT); do
            if [ $i -le $REQUESTS ]; then
                file="stress_test_$i.dat"
                curl -s -F "file=@$file" "$SERVER_URL" > "stress_result_$i.json" 2>&1 &
                pids[$j]=$!
                i=$((i + 1))
            fi
        done
        
        for pid in "${pids[@]}"; do
            wait $pid 2>/dev/null
        done
        unset pids
    done
    
    end_time=$(date +%s.%N)
    duration=$(echo "$end_time - $start_time" | bc)
    
    # 停止监控
    kill $SAR_PID 2>/dev/null
    
    # 统计结果
    success_count=0
    for i in $(seq 1 $REQUESTS); do
        if [ -f "stress_result_$i.json" ] && grep -q "success" "stress_result_$i.json"; then
            success_count=$((success_count + 1))
        fi
    done
    
    # 检查实际文件
uploaded_files=$(find "$UPLOAD_DIR" -name "*stress_test*" -type f 2>/dev/null | wc -l)
    
    echo "📊 结果:"
    echo "  耗时: $duration 秒"
    echo "  成功率: $success_count/$REQUESTS"
    echo "  实际上传: $uploaded_files"
    echo "  吞吐量: $(echo "scale=2; $REQUESTS / $duration" | bc) 请求/秒"
    echo ""
    
    # 清理
    rm -f stress_test_*.dat stress_result_*.json
    sleep 2  # 给系统恢复时间
done

echo "=== 压力测试完成 ==="