
all: midorator.so

midorator.so: midorator.c midorator.h uzbl-follow.h go-next.h default.h
	$(CC) midorator.c -Iincludes -o midorator.so -shared $(shell pkg-config gtk+-2.0 webkit-1.0 --cflags --libs) $(CFLAGS) $(LDFLAGS)

uzbl-follow.h: uzbl-follow.js
	sed 's/"/\\"/g;s/%[^s]/%&/g;s/.*/"&\\n"/' uzbl-follow.js > uzbl-follow.h

go-next.h: go-next.js
	sed 's/"/\\"/g;s/%[^s]/%&/g;s/.*/"&\\n"/' go-next.js > go-next.h

default.h: default.config
	sed 's/"/\\"/g;s/%/%%/g;s/.*/\tmidorator_process_command(web_view, "&");/' default.config > default.h

