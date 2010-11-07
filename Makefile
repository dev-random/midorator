
all: midorator.so

midorator.o: midorator.c midorator.h default.h midorator-entry.h
	$(CC) -c $< -Iincludes -o $@ -fPIC -shared $(shell pkg-config gtk+-2.0 webkit-1.0 --cflags) $(CFLAGS)

midorator-entry.o: midorator-entry.c midorator-entry.h
	$(CC) -c $< -Iincludes -o $@ -fPIC -shared $(shell pkg-config gtk+-2.0 webkit-1.0 --cflags) $(CFLAGS)

midorator.so: midorator.o midorator-entry.o
	$(CC) $^ -o $@ -fPIC -shared $(shell pkg-config gtk+-2.0 webkit-1.0 --libs) $(LDFLAGS)

default.h: default.config
	sed 's/"/\\"/g;s/.*/\t"&",/' default.config > default.h

debug:
	$(MAKE) CFLAGS='-ggdb3 -DDEBUG -O0 -rdynamic' midorator.so

