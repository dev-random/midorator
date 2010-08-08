
all: midorator.so

midorator.so: midorator.c midorator.h uzbl-follow.h go-next.h
	$(CC) midorator.c -Iincludes -o midorator.so -shared $(shell pkg-config gtk+-2.0 webkit-1.0 --cflags --libs) $(CFLAGS) $(LDFLAGS)

uzbl-follow.h: uzbl-follow.js
	sed 's/"/\\"/g;s/%[^s]/%&/g;s/.*/"&\\n"/' uzbl-follow.h > uzbl-follow.js

go-next.h: go-next.js
	sed 's/"/\\"/g;s/%[^s]/%&/g;s/.*/"&\\n"/' go-next.h > go-next.js

