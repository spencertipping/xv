XV_CC_OPTS = -Wall -Wno-parentheses -std=gnu99

all: build/xv-x64.o

build/%.o: build/%.c makefile
	$(CC) $(CC_OPTS) $(XV_CC_OPTS) $< -c -o $@

build/%.c: %.c.sdoc
	mkdir -p build
	rm -f $@
	sdoc cat code.c::$< > $@
	chmod -w $@

build/%.h: %.h.sdoc
	rm -f $@
	sdoc cat code.h::$< > $@
	chmod -w $@

build/xv-x64.c: build/xv-x64.h
build/xv-x64.h: build/xv.h
