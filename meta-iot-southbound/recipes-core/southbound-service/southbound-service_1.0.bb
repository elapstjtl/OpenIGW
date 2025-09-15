SUMMARY = "Southbound service - plugin manager and main service"
DESCRIPTION = "Main service that loads and manages southbound protocol adapters"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# 依赖项
DEPENDS = "southbound-api"
RDEPENDS:${PN} = "southbound-api"

# 源码文件
SRC_URI = "file://project/"

S = "${WORKDIR}/project"

# 使用 cmake 构建
inherit cmake pkgconfig

# 编译标志
EXTRA_OECMAKE += "-DCMAKE_BUILD_TYPE=Release -DPV=${PV}"

# 安装目录
PLUGIN_DIR = "${libdir}/southbound/plugins"
CONFIG_DIR = "${sysconfdir}/southbound"

# 安装规则
do_install:append() {
    # 创建插件目录
    install -d ${D}${PLUGIN_DIR}
    
    # 创建配置目录
    install -d ${D}${CONFIG_DIR}
    
    # 安装示例配置文件
    install -m 0644 ${S}/config/*.conf ${D}${CONFIG_DIR}/
}

# 打包
FILES:${PN} += "${bindir}/southbound-service"
FILES:${PN} += "${PLUGIN_DIR}"
FILES:${PN} += "${CONFIG_DIR}"

# 开发包依赖
RDEPENDS:${PN}-dev = "southbound-api-dev"
