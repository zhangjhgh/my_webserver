# my_webserver

轻量级Web服务器实现，基于C/C++开发，支持基础HTTP服务、文件上传功能，并提供完善的测试脚本（性能测试、压力测试、并发上传测试等），可快速验证服务器的功能与性能表现。

## 目录结构说明
my_webserver/├── .gitignore # 忽略编译产物、IDE 配置、日志等文件 / 目录├── CMakeLists.txt # CMake 构建配置文件├── *.sh # 各类测试 / 运行脚本（详见「测试说明」）├── test_report.md # 测试报告文档├── src/server/ # 服务器核心源码目录├── include/server/ # 服务器头文件目录├── uploads/ # 文件上传存储目录（存放测试用上传文件）└── build/（编译生成） # 构建产物目录（默认被.gitignore 忽略）
plaintext

## 环境要求
- 操作系统：Linux（推荐Ubuntu/CentOS等，脚本均为Shell脚本）
- 编译工具：gcc/clang（支持C/C++11+）、CMake 3.10+
- 依赖：无额外第三方依赖（原生系统API实现）

## 编译构建
1. 克隆仓库到本地
   ```bash
   git clone <仓库地址>
   cd my_webserver
创建构建目录并编译
bash
运行
# 创建build目录（已被.gitignore忽略）
mkdir build && cd build
# 生成Makefile
cmake ..
# 编译项目（多核编译提升速度）
make -j$(nproc)
编译产物会生成在build/或bin/目录下（取决于 CMake 配置）。
运行服务器
快速运行
直接执行项目根目录的运行脚本：
bash
运行
./run.sh
手动运行
编译完成后，进入构建产物目录执行服务器二进制文件：
bash
运行
cd build/bin  # 或对应产物路径
./my_webserver  # 服务器二进制文件名（需与CMake配置一致）
测试说明
项目提供多类测试脚本，覆盖功能、性能、稳定性等维度，可按需执行：
表格
脚本名称	用途说明
run.sh	服务器快速启动脚本
run_all_tests.sh	一键执行所有测试用例（功能 + 性能）
test.sh	基础功能测试（验证 HTTP 请求、响应、文件上传等核心流程）
stress_test.sh	压力测试（高并发请求下验证服务器稳定性）
upload_concurrent.sh	并发文件上传测试（验证多客户端同时上传文件的处理能力）
performance_monitor_fixed.sh	性能监控测试（统计服务器 QPS、响应时间、资源占用等指标）
large_file_test.sh	大文件上传测试（验证大尺寸文件上传的完整性与性能）
error_test.sh	异常场景测试（验证无效请求、断连、文件权限等异常的容错能力）
generate_report.sh	测试报告生成脚本（汇总测试结果，输出 test_report.md）
执行测试
bash
运行
# 执行所有测试
./run_all_tests.sh

# 仅执行并发上传测试
./upload_concurrent.sh
.gitignore 说明
仓库忽略以下类型文件，避免提交无关内容：
编译产物：*.o、*.exe、*.out 等
构建目录：build/、bin/、cmake-build-*/ 等
IDE 配置：.vscode/、.idea/、*.swp 等
日志 / 临时文件：*.log、tmp/、temp/ 等
系统文件：.DS_Store（MacOS）
许可证
本项目暂未指定开源许可证，可根据实际需求补充（如 MIT、Apache 2.0 等）。
注意事项
测试前确保服务器已正常启动（部分测试脚本会自动启动 / 停止服务器）；
uploads/ 目录用于存储测试上传的文件，测试后可手动清理；
性能测试结果与运行环境（CPU、内存、网络）强相关，建议在相同环境下对比测试。
plaintext

### 总结
1. 该README完整覆盖了轻量级C/C++ Web服务器的核心使用流程，包括环境要求、编译构建、运行和多维度测试；
2. 格式规范且可直接复制粘贴，包含目录结构、脚本说明、注意事项等实用信息；
3. 需替换`<仓库地址>`为实际的仓库克隆地址，可根据需求补充许可证类型。
