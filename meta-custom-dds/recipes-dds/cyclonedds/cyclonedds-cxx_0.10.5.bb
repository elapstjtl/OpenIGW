# meta-your-layer/recipes-dds/cyclonedds/cyclonedds-cxx_0.9.0.bb

SUMMARY = "Eclipse Cyclone DDS C++ Language Binding"
DESCRIPTION = "The C++ language binding for Eclipse Cyclone DDS. It provides a modern, idiomatic C++ API."
HOMEPAGE = "https://github.com/eclipse-cyclonedds/cyclonedds-cxx"
LICENSE = "EPL-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=ca2dafd1f07f3cd353d0454d3c4d9e80"

# 远程 Git 仓库地址
SRC_URI = "git://github.com/eclipse-cyclonedds/cyclonedds-cxx.git;protocol=https;branch=master"

# latest commit hash
SRCREV = "e304e4ddbd325fa1baf15ccb261d5ddfc69a492e"

S = "${WORKDIR}/git"

inherit cmake

# 这是最关键的部分：声明此配方依赖于上一个配方（cyclonedds）。
# Bitbake 会确保在构建此包之前，cyclonedds 已经被成功构建并安装到 sysroot 中。
DEPENDS = "cyclonedds cyclonedds-native"

# CMake 参数，关闭示例和测试的构建
EXTRA_OECMAKE = "\
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_TESTING=OFF \
    -DCycloneDDS_IDLC_EXECUTABLE=${STAGING_BINDIR_NATIVE}/idlc \
"