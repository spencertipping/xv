XV_CC_OPTS = -Wall -Wno-parentheses -pedantic -std=gnu99

all: xv-x64.o

%.o: %.c makefile
	$(CC) $(CC_OPTS) $(XV_CC_OPTS) $< -c -o $@

%.c: %.c.sdoc
	rm -f $@
	sdoc cat code.c::$< > $@
	chmod -w $@

%.h: %.h.sdoc
	rm -f $@
	sdoc cat code.h::$< > $@
	chmod -w $@

xv-x64.c: xv-x64.h
xv-x64.h: xv.h
