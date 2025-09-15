cmake_minimum_required(VERSION 3.10)
project(modbus-adapter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖包
find_package(PkgConfig REQUIRED)
pkg_check_modules(SOUTHBOUND_API REQUIRED southbound-api)
pkg_check_modules(LIBMODBUS REQUIRED libmodbus)

# 包含目录
include_directories(${SOUTHBOUND_API_INCLUDE_DIRS})
include_directories(${LIBMODBUS_INCLUDE_DIRS})

# 源文件
set(SOURCES
    src/ModbusAdapter.cpp
    src/ModbusAdapterFactory.cpp
)

# 创建共享库
add_library(modbus-adapter SHARED ${SOURCES})

# 链接库
target_link_libraries(modbus-adapter 
    ${SOUTHBOUND_API_LIBRARIES}
    ${LIBMODBUS_LIBRARIES}
)

# 设置编译选项
target_compile_options(modbus-adapter PRIVATE 
    ${SOUTHBOUND_API_CFLAGS_OTHER}
    ${LIBMODBUS_CFLAGS_OTHER}
)

# 设置输出目录和 SONAME
set_target_properties(modbus-adapter PROPERTIES
    SOVERSION 1
)

# 生成并安装 pkg-config 文件
include(GNUInstallDirs)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/modbus-adapter.pc.in ${CMAKE_BINARY_DIR}/modbus-adapter.pc @ONLY)

# 安装规则
install(TARGETS modbus-adapter
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/southbound/plugins
)
install(FILES ${CMAKE_BINARY_DIR}/modbus-adapter.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
