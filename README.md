# my_webserver

一个基于 C++ 实现的轻量级 Web 服务器项目，主要用于学习和实践 Linux 网络编程、高并发服务器架构以及 HTTP 协议处理。

该服务器支持基础 HTTP 请求处理、静态资源访问以及文件上传功能，并提供多种自动化测试脚本，用于验证服务器在并发场景下的稳定性与性能表现。

---

## 项目简介

my_webserver 是一个基于 Linux Socket 编程实现的 Web 服务器项目，使用 C++ 编写，通过 Epoll IO 多路复用实现高效的网络事件处理。

项目主要用于实践以下内容：

- Linux 网络编程
- HTTP 协议解析
- 高并发服务器架构
- 后端服务开发实践

---

## 项目特点

- 基于 **C++11** 开发
- 使用 **Epoll IO 多路复用** 实现高并发处理
- 支持 **HTTP 请求解析与静态文件响应**
- 支持 **文件上传功能**
- 使用 **非阻塞 Socket** 提升并发连接处理能力
- 提供多种 **自动化测试脚本**
- 使用 **CMake 构建项目**

---

## 项目结构

```
my_webserver
├── src/ # 服务器核心源码
├── include/ # 头文件
├── uploads/ # 上传文件存储目录
├── build/ # 编译生成目录
├── test scripts/ # 各类测试脚本
├── CMakeLists.txt # CMake 构建文件
└── README.md # 项目说明
```

---

## 运行环境

操作系统：

- Linux（推荐 Ubuntu / CentOS）

编译环境：

- g++ 或 clang（支持 C++11 及以上）
- CMake ≥ 3.10

---


## 编译项目

```bash
git clone https://github.com/zhangjhgh/my_webserver.git
cd my_webserver

mkdir build
cd build

cmake ..
make
```

## 运行服务器

可以使用脚本快速运行：

```bash
./run.sh

或手动运行服务器程序：

./my_webserver
```

## 测试脚本

项目提供多种测试脚本：

- `test.sh` 基础功能测试
- `stress_test.sh` 高并发压力测试
- `upload_concurrent.sh` 并发上传测试
- `large_file_test.sh` 大文件上传测试
- `error_test.sh` 异常请求测试
- `performance_monitor_fixed.sh` 性能监控
- `run_all_tests.sh` 一键执行全部测试

执行全部测试：

```bash
./run_all_tests.sh

测试结果会生成在：

test_report.md
```

## 学习内容

通过本项目可以学习到：

- Linux Socket 网络编程
- Epoll IO 多路复用
- HTTP 协议解析
- 多线程并发处理
- Web 服务器基本架构设计
- Linux 环境下服务器开发
