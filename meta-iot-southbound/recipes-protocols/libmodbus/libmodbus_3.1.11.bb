#
# Yocto recipe for libmodbus
#
SUMMARY = "A C library for the Modbus protocol"
DESCRIPTION = "libmodbus is a free software library to send/receive data with a device which respects the Modbus protocol. This library can use a serial line or an Ethernet network to communicate."
HOMEPAGE = "http://libmodbus.org"
SECTION = "libs"

# libmodbus 使用 LGPL-2.1-or-later 许可证
LICENSE = "LGPL-2.1-or-later"

# LIC_FILES_CHKSUM 指向源代码中包含许可证文本的文件，并计算其校验和
# 这是为了确保许可证没有被意外更改，是 Yocto 的标准合规性要求
LIC_FILES_CHKSUM = "file://COPYING.LESSER;md5=4fbd65380cdd255951079008b364516c"

# S 指向本地源码目录
S = "${EXTERNALSRC}"

# 源码本地路径（请确保该路径已包含可用的构建脚本，如 configure.ac/Makefile.am 等）
EXTERNALSRC = "${THISDIR}/libmodbus"
# 使用独立的 out-of-tree 构建目录
EXTERNALSRC_BUILD = "${WORKDIR}/build"

# 继承 autotools 提供标准构建流程，并使用 externalsrc 从本地源码构建
inherit autotools pkgconfig externalsrc

# EXTRA_OECONF 允许我们向 ./configure 脚本传递额外的参数
# 在嵌入式环境中，我们通常不需要编译和安装测试程序或文档，这样可以节省空间和编译时间
EXTRA_OECONF += "--disable-tests"

#
# 不需要手动编写 do_configure, do_compile, do_install 任务，
# 因为 'autotools' 类已经为我们提供了标准的实现。
#
# Yocto 的打包系统会自动将生成的文件分割到不同的包中：
# - ${PN}: 主包，包含运行时所需的动态链接库 (e.g., /usr/lib/libmodbus.so.5)
# - ${PN}-dev: 开发包，包含头文件和符号链接，用于编译其他依赖此库的软件
# - ${PN}-dbg: 调试包，包含调试符号
# - ${PN}-staticdev: 静态链接库包