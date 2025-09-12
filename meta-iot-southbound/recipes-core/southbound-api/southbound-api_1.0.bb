SUMMARY = "Southbound adapter API (header-only)"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI += " \
            file://Inc/ \
            file://southbound-api.pc.in \
"

S = "${UNPACKDIR}"

# 仅安装头文件与 pkg-config 文件
inherit allarch

do_install() {
	install -d ${D}${includedir}/southbound
	install -m 0644 ${S}/Inc/*.hpp ${D}${includedir}/southbound/

	install -d ${D}${libdir}/pkgconfig
	sed "s|@PV@|${PV}|g" ${S}/southbound-api.pc.in > ${D}${libdir}/pkgconfig/southbound-api.pc
}

# 将头文件与 .pc 放入主包，简化使用
FILES:${PN} += "${includedir} ${libdir}/pkgconfig/southbound-api.pc"
