# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=3

EGIT_REPO_URI="https://github.com/dev-random/midorator.git"
inherit git

DESCRIPTION="Vimperator-like extension for Midori"
HOMEPAGE="https://github.com/dev-random/midorator"
SRC_URI=""

LICENSE="LGPL-2.1"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE="debug"

DEPEND="www-client/midori"
RDEPEND="${DEPEND}"

src_compile() {
	if use debug
	then
		emake CFLAGS="$CFLAGS -ggdb3 -DDEBUG -O0 -rdynamic"
	else
		emake
	fi
}

src_install() {
	exeinto /usr/lib/midori
	doexe midorator.so
}

