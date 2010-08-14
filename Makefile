
all: midorator.so

midorator.so: midorator.c midorator.h default.h
	$(CC) midorator.c -Iincludes -o midorator.so -shared $(shell pkg-config gtk+-2.0 webkit-1.0 --cflags --libs) $(CFLAGS) $(LDFLAGS)

default.h: default.config
	sed 's/"/\\"/g;s/%/%%/g;s/.*/\tmidorator_process_command(web_view, "&");/' default.config > default.h

