my_webserver 项目说明文档

1 项目简介

my_webserver 是一个基于 C++11 与 Linux Socket 网络编程实现的轻量级 Web 服务器项目。

服务器支持基础 HTTP 请求处理、静态资源访问以及文件上传功能，并提供多种自动化测试脚本，用于验证服务器在并发场景下的稳定性与性能。

本项目主要用于学习和实践：

Linux Socket 网络编程  
HTTP 协议解析  
多线程并发处理  
Web 服务器基本架构设计  

2 项目特点

- 基于 Epoll IO 多路复用实现高效网络事件处理
- 使用非阻塞 Socket 提升并发连接处理能力
- 支持 HTTP 请求解析与静态文件响应
- 支持文件上传功能
- 提供并发上传测试与压力测试脚本
- 使用 CMake 进行项目构建
- 无第三方依赖，仅使用 C++11 + Linux 系统 API

3 项目结构

src/server        服务器核心源码  
include/server    服务器头文件  
uploads           上传文件目录  
CMakeLists.txt    构建配置  
run.sh            启动脚本  
stress_test.sh    压力测试脚本  
upload_concurrent.sh 并发上传测试  

4 运行环境

操作系统：Linux（Ubuntu / CentOS）

编译环境：

g++ 或 clang（支持 C++11）  
CMake ≥ 3.10  

5 编译运行

git clone <仓库地址>  
cd my_webserver  

mkdir build  
cd build  

cmake ..  
make  

运行服务器：

./run.sh
