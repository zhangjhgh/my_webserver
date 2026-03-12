#!/bin/bash
# run_all_tests.sh - 完整测试执行流程

echo "=== 开始完整服务器测试 ==="

# 1. 启动性能监控
echo "1. 启动性能监控..."
./performance_monitor_fixed.sh &
MONITOR_PID=$!
echo "监控进程ID: $MONITOR_PID"

# 等待监控启动
sleep 2

# 2. 运行压力测试
echo "2. 运行压力测试..."
./stress_test.sh

# 3. 运行大文件测试
echo "3. 运行大文件测试..."
./large_file_test.sh

# 4. 运行错误处理测试
echo "4. 运行错误处理测试..."
./error_test.sh

# 5. 停止性能监控
echo "5. 停止性能监控..."
kill $MONITOR_PID
wait $MONITOR_PID 2>/dev/null

# 6. 生成测试报告
echo "6. 生成测试报告..."
./generate_report.sh > test_report.md

echo "=== 所有测试完成 ==="
echo "测试报告已保存到: test_report.md"