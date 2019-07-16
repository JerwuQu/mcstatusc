# Maintainer: Marcus Ramse <marcus@ramse.se>
pkgname=mcstatusc-git
pkgdesc='Minecraft Server Status'
pkgver=master
pkgrel=1
arch=(x86_64)
source=('git+https://github.com/JerwuQu/mcstatusc')
sha256sums=('SKIP')
_extracted="${pkgname}-${pkgver}"

pkgver() {
	cd "${srcdir}/${pkgname%-git}"
	git rev-parse --short HEAD
}

build () {
	cd "${srcdir}/${pkgname%-git}"
	make
}

package () {
	cd "${srcdir}/${pkgname%-git}"
	mkdir -p "${pkgdir}/usr/local/bin"
	mv "mcstatusc" "${pkgdir}/usr/local/bin/mcstatusc"
}
