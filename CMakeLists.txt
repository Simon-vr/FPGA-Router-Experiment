cmake_minimum_required(VERSION 3.10)
project(MyRouterProject LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # 禁用编译器特定扩展，保持标准兼容性

# 1. 收集源文件
# GLOB_RECURSE 会递归查找，但为了精确控制，我们手动指定两个目录
file(GLOB SRC_FILES 
    "*.cpp"          # 所有 cpp
    "vistual/*.cpp"     # vistual 目录的所有 cpp
)

# 2. 包含头文件路径
# 这样你在代码里可以直接 #include "xxx.h" 而不用写相对路径
include_directories(
    .                   # 目录
    vistual             # vistual 目录
)

# 3. 定义可执行文件
add_executable(main ${SRC_FILES})

# 4. 查找并链接 OpenMP
find_package(OpenMP REQUIRED)
if(OpenMP_CXX_FOUND)
    target_link_libraries(main PUBLIC OpenMP::OpenMP_CXX)
    target_compile_options(main PUBLIC ${OpenMP_CXX_FLAGS})
    message(STATUS "OpenMP found: ${OpenMP_CXX_FLAGS}")
else()
    message(WARNING "OpenMP not found!")
endif()

# 5. 编译选项 (相当于 Makefile 里的 CFLAGS)
target_compile_options(main PRIVATE
    -Wall
    -Wextra
    -pedantic
)

# 如果是 Debug 模式，自动添加 -g
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)