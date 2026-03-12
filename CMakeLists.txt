cmake_minimum_required(VERSION 3.10)
project(MyWebServer VERSION 1.0.0)

#设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# 设置输出路径并自动创建目录
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH})

# 自动递归搜集 src/ 下的所有 cpp 文件
file(GLOB_RECURSE SOURCES
    ${PROJECT_SOURCE_DIR}/src/*.cpp
)

# 创建可执行文件
add_executable(webserver ${SOURCES})

# 包含头文件目录（现代语法）
target_include_directories(webserver PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_include_directories(webserver PRIVATE ${PROJECT_SOURCE_DIR}/src/server)

# 2. 编译 flags：保留 -pthread，确保编译阶段启用线程支持
target_compile_options(webserver PRIVATE -g -Wall -Wextra -pthread)

# 3. 链接线程库：用 CMAKE_THREAD_LIBS_INIT（兼容旧版 CMake，自动展开为 -pthread 或 -lpthread）
target_link_libraries(webserver PRIVATE ${CMAKE_THREAD_LIBS_INIT})