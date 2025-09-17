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
# 对于目标架构，不构建 IDLC 工具，避免CMake配置冲突
EXTRA_OECMAKE = "\
    -DBUILD_TESTING=OFF \
    -DENABLE_SSL=YES \
    -DBUILD_IDLC=OFF \
"

# 对于native版本，构建 IDLC 工具
EXTRA_OECMAKE:class-native = "\
    -DBUILD_TESTING=OFF \
    -DENABLE_SSL=YES \
    -DBUILD_IDLC=YES \
"

# 解决 QA 错误：跳过 dev-elf 检查
# CycloneDDS 的安全插件库 (libdds_security_*.so) 是运行时库，不是开发文件
# 但 BitBake 的自动分割逻辑错误地将它们分配到 dev 包中
# 使用 INSANE_SKIP 是处理这种特殊情况的标准做法
INSANE_SKIP:${PN}-dev += "dev-elf"

# 只在native版本中包含idlc工具
PACKAGES =+ "${PN}-idlc"
FILES:${PN}-idlc = "${bindir}/idlc"
ALLOW_EMPTY:${PN}-idlc = "1"

# 目标架构版本中不包含idlc
FILES:${PN}-idlc:class-target = ""

# 构建时工具需要是主机架构，而不是目标架构
BBCLASSEXTEND = "native"