
all: midorator.so

midorator.so: midorator.c midorator.h default.h
	$(CC) midorator.c -Iincludes -o midorator.so -fPIC -shared $(shell pkg-config gtk+-2.0 webkit-1.0 --cflags --libs) $(CFLAGS) $(LDFLAGS)

default.h: default.config
	sed 's/"/\\"/g;s/.*/\t"&",/' default.config > default.h

debug:
	$(MAKE) CFLAGS='-ggdb3 -DDEBUG -O0 -rdynamic' midorator.so

