# meta-your-layer/recipes-dds/cyclonedds/cyclonedds_0.10.5.bb

SUMMARY = "Eclipse Cyclone DDS, an implementation of the OMG Data Distribution Service (DDS) specification"
DESCRIPTION = "Eclipse Cyclone DDS is a very performant and robust open-source DDS implementation."
HOMEPAGE = "https://cyclonedds.io/"
LICENSE = "EPL-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=ca2dafd1f07f3cd353d0454d3c4d9e80"

# 远程 Git 仓库地址
SRC_URI = "git://github.com/eclipse-cyclonedds/cyclonedds.git;protocol=https;branch=master"

# latest commit hash
SRCREV = "da10d20a081db333dcd541fe7538f6d981c8c7f5"

# S (Source Directory) 指向 bitbake 从 git clone 源码后存放的位置
S = "${WORKDIR}/git"

inherit cmake

# 声明编译依赖项，例如 openssl
DEPENDS = "openssl"

# 配置 CMake 参数以适应嵌入式构建
# BUILD_IDLC=YES 表示我们希望构建并安装 IDL 代码生成工具
EXTRA_OECMAKE = "\
    -DBUILD_TESTING=OFF \
    -DENABLE_SSL=YES \
    -DBUILD_IDLC=YES \
"

# 如果需要，可以分包。例如，将 idlc 工具单独打包。
PACKAGES =+ "${PN}-idlc"
FILES:${PN}-idlc = "${bindir}/idlc"

# 解决 QA 错误：将安全库文件分配到主包中
# 这些是 cyclonedds 的安全插件库，应该在运行时包中，而不是 dev 包中
FILES:${PN} += "\
    ${libdir}/libdds_security_ac.so \
    ${libdir}/libdds_security_crypto.so \
    ${libdir}/libdds_security_auth.so \
"

# 构建时工具需要是主机架构，而不是目标架构
BBCLASSEXTEND = "native"