# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=3

AUTHOR=dev-random
SRC_URI=""
if [ "$PV" == 9999 ]; then
	EGIT_REPO_URI="git://github.com/$AUTHOR/$PN.git"
	inherit git
else
	SRC_URI="https://github.com/$AUTHOR/$PN/tarball/$PV -> $P.tar.gz"
fi

DESCRIPTION="Vimperator-like extension for Midori"
HOMEPAGE="https://github.com/dev-random/midorator"

LICENSE="LGPL-2.1"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE="debug doc"

DEPEND="doc? ( app-text/asciidoc )"
RDEPEND="www-client/midori"

src_compile() {
	S=$(ls -d $WORKDIR/*/)
	cd "$S"
	if use debug; then
		emake CFLAGS="$CFLAGS -ggdb3 -DDEBUG -O0 -rdynamic" || die
	else
		emake || die
	fi
	if use doc; then
		emake doc || die
	fi
}

src_install() {
	emake install DESTDIR="$D"
	if use doc; then
		emake install-doc DESTDIR="$D" DOCDIR="/usr/share/doc/$PF/" || die
	fi
}

