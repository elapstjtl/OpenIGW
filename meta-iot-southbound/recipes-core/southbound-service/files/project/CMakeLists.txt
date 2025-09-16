cmake_minimum_required(VERSION 3.10)
project(southbound-service)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖包
find_package(PkgConfig REQUIRED)
pkg_check_modules(SOUTHBOUND_API REQUIRED southbound-api)

# 新增：标准安装目录变量
include(GNUInstallDirs)

# 新增：线程库
find_package(Threads REQUIRED)

# 包含目录
include_directories(${SOUTHBOUND_API_INCLUDE_DIRS})
include_directories(Inc)

# 源文件
set(SOURCES
    src/main.cpp
    src/SouthboundService.cpp
    src/PluginManager.cpp
    src/ConfigManager.cpp
)

# 创建可执行文件
add_executable(southbound-service ${SOURCES})

# 链接库
target_link_libraries(southbound-service 
    ${SOUTHBOUND_API_LIBRARIES}
    dl  # 用于动态库加载
    Threads::Threads
)

# 设置编译选项
target_compile_options(southbound-service PRIVATE 
    ${SOUTHBOUND_API_CFLAGS_OTHER}
)

# 安装规则
install(TARGETS southbound-service
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
