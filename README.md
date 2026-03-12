# 1. 项目基础信息
名称：高性能C++ Web服务器
用途：基于Reactor+Epoll实现的轻量级Web服务器，支持HTTP/1.1解析、高并发连接处理，无内存泄漏

# 2. 技术栈细节
C++11、Linux Socket、Epoll ET模式、手写线程池（无第三方线程池库）、RAII资源管理
无额外第三方库（纯原生C+++Linux API）

# 3. 代码目录结构
my_webserver/
├── src/
│   ├── main.cpp       // 入口，初始化服务器+启动监听
│   ├── http_parser.cpp/h  // HTTP请求解析
│   ├── epoll_wrapper.cpp/h // Epoll封装
│   ├── thread_pool.cpp/h  // 线程池实现
│   └── connection.cpp/h   // 客户端连接管理
├── include/           // 所有头文件统一存放
├── CMakeLists.txt     // 基础cmake配置，指定C++11、链接pthread
└── ab_test.sh         // ab压测脚本

# 4. 编译&运行步骤
- 安装依赖：sudo apt install cmake g++
- 编译：
  mkdir build && cd build
  cmake ..
  make -j4
- 启动：./webserver 8080 （8080为监听端口，可自定义）

# 5. 核心功能实现
- 支持HTTP/1.1 GET请求解析，返回静态页面/字符串响应
- Epoll ET模式+非阻塞Socket，主线程监听IO，线程池处理业务
- RAII封装Socket、互斥锁，Valgrind检测无内存泄漏
- 压测命令：ab -n 10000 -c 3000 http://192.168.1.100:8080/

# 6. 其他关键信息
- 测试环境：Ubuntu 20.04 4核8G
- 默认端口8080，需确保端口未被占用
- 仅支持Linux，无Windows适配代码
