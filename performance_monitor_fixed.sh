#!/bin/bash
# performance_monitor_fixed.sh

UPLOAD_DIR="/home/zhangsan/~projects/my_webserver/uploads"
LOG_FILE="performance_$(date +%Y%m%d_%H%M%S).log"

echo "=== 服务器性能监控 ==="
echo "监控日志: $LOG_FILE"

monitor_performance() {
    echo "时间戳,CPU使用%,内存使用%,活跃连接数,磁盘使用%" > "$LOG_FILE"
    
    while true; do
        timestamp=$(date +%Y-%m-%d\ %H:%M:%S)
        
        # CPU使用率
        cpu_usage=$(top -bn1 | grep "Cpu(s)" | awk '{print $2}' | cut -d'%' -f1)
        
        # 内存使用率（修复版）
        mem_usage=$(free | awk 'NR==2{printf "%.1f", $3*100/$2}')
        
        # 磁盘使用率
        disk_usage=$(df / | awk 'NR==2 {print $5}' | cut -d'%' -f1)
        
        # 活跃连接数
        active_connections=$(ss -tln sport = :8080 2>/dev/null | grep -c ESTAB)
        
        echo "$timestamp,$cpu_usage,$mem_usage,$active_connections,$disk_usage" >> "$LOG_FILE"
        
        echo "🖥️  监控中... CPU: ${cpu_usage}%, 内存: ${mem_usage}%, 连接: $active_connections, 磁盘: ${disk_usage}%"
        sleep 5
    done
}

monitor_performance