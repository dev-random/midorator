
PREFIX = /usr
DOCDIR = $(PREFIX)/share/doc/midorator/
MIDORI_LIBDIR = $(PREFIX)/lib/midori/
ASCIIDOC = /usr/bin/asciidoc
A2X = /usr/bin/a2x
INSTALL = /usr/bin/install
GIT_REV = HEAD
MIDORATOR_VERSION := $(shell sed -n '/VERSION/{s/.* \"//;s/\".*//;p}' midorator.h)
DESTDIR =

all: midorator.so

midorator.o: midorator.c midorator.h default.h midorator-entry.h midorator-history.h midorator-commands.h
	$(CC) -c $< -Iincludes -o $@ -fPIC -shared $(shell pkg-config gtk+-2.0 webkit-1.0 --cflags) $(CFLAGS)

midorator-entry.o: midorator-entry.c midorator-entry.h
	$(CC) -c $< -Iincludes -o $@ -fPIC -shared $(shell pkg-config gtk+-2.0 webkit-1.0 --cflags) $(CFLAGS)

midorator-commands.o: midorator-commands.c midorator-commands.h
	$(CC) -c $< -Iincludes -o $@ -fPIC -shared $(shell pkg-config gtk+-2.0 webkit-1.0 --cflags) $(CFLAGS)

midorator-message.o: midorator-message.c midorator-message.h
	$(CC) -c $< -Iincludes -o $@ -fPIC -shared $(shell pkg-config gtk+-2.0 webkit-1.0 --cflags) $(CFLAGS)

midorator-history.o: midorator-history.c midorator-history.h
	$(CC) -c $< -Iincludes -o $@ -fPIC -shared $(shell pkg-config gtk+-2.0 webkit-1.0 sqlite3 --cflags) $(CFLAGS)

midorator.so: midorator.o midorator-entry.o midorator-history.o midorator-commands.o midorator-message.o
	$(CC) $^ -o $@ -fPIC -shared $(shell pkg-config gtk+-2.0 webkit-1.0 sqlite3 --libs) $(LDFLAGS)

default.h: default.config
	sed 's/"/\\"/g;s/.*/\t"&",/' default.config > default.h

debug:
	$(MAKE) CFLAGS='-ggdb3 -DDEBUG -O0 -rdynamic' midorator.so

doc: README.html midorator.7

README.html: README.asciidoc
	$(ASCIIDOC) -o $@ $<

midorator.7: README.asciidoc
	$(A2X) -L -f manpage $<
	test -e $@

install: all
	$(INSTALL) -d $(DESTDIR)$(MIDORI_LIBDIR)
	$(INSTALL) midorator.so $(DESTDIR)$(MIDORI_LIBDIR)

clean:
	rm -f *.o *.so README.html midorator.7

archive:
	git archive --prefix=midorator-$(MIDORATOR_VERSION)/ $(GIT_REV) | gzip > ../midorator_$(MIDORATOR_VERSION).orig.tar.gz

install-doc: doc
	$(INSTALL) -d $(DESTDIR)/usr/share/man/man7/ $(DESTDIR)$(DOCDIR)
	$(INSTALL) midorator.7 $(DESTDIR)/usr/share/man/man7/
	$(INSTALL) README.html $(DESTDIR)$(DOCDIR)





.PHONY: all debug doc install install-doc clean archive

