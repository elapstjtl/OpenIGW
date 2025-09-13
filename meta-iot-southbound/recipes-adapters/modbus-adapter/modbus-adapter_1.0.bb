SUMMARY = "Modbus protocol adapter for southbound framework"
DESCRIPTION = "A Modbus RTU/TCP adapter that implements the southbound API interface for device communication"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# 依赖项
DEPENDS = "southbound-api libmodbus"
RDEPENDS:${PN} = "libmodbus"

# 源码文件
SRC_URI = "file://project/src/ \
           file://project/CMakeLists.txt \
           file://project/modbus-adapter.pc.in \
           file://README.md"

S = "${WORKDIR}/project"

# 使用 cmake 构建
inherit cmake pkgconfig

# 编译标志
EXTRA_OECMAKE += "-DCMAKE_BUILD_TYPE=Release -DPV=${PV}"

# 打包
FILES:${PN} += "${libdir}/libmodbus-adapter.so.*"
FILES:${PN}-dev += "${libdir}/libmodbus-adapter.so ${libdir}/pkgconfig/modbus-adapter.pc"

# 开发包依赖
RDEPENDS:${PN}-dev = "southbound-api-dev libmodbus-dev"
