# 并发数量（根据服务器承载能力调整）
CONCURRENT=5

# 循环启动并发任务
for ((i=1; i<=CONCURRENT; i++)); do
  echo "Starting upload $i..."
  # 后台执行curl命令，输出日志到单独文件（可选）
  curl -F "file=@/home/zhangsan/桌面/test.mp4" http://localhost:8080/upload \
    > "upload_$i.log" 2>&1 &
done

# 等待所有后台进程结束
wait
echo "All uploads completed!"