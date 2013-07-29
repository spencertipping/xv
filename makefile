XV_CC_OPTS = -g -Wall -Wno-parentheses -Wno-unused-value -std=gnu99
XV_OBJ     = build/xv-x64.o
XV_TEST    = test/disasm test/disasm.bin
XV_DOC     = $(subst .sdoc,.md,$(wildcard *.sdoc))

all: $(XV_TEST) $(XV_DOC)
doc: $(XV_DOC)

test/%: test/%.c $(XV_OBJ)
	$(CC) $(CC_OPTS) $(XV_CC_OPTS) $< $(XV_OBJ) -o $@

test/%.bin: test/%.o
	objcopy -O binary -j .text $< $@

%.o: %.c makefile
	$(CC) $(CC_OPTS) $(XV_CC_OPTS) $< -c -o $@

%.md: %.sdoc
	sdoc cat markdown::$< > $@

build/%.c: %.c.sdoc
	mkdir -p build
	rm -f $@
	sdoc cat code.c::$< > $@
	chmod -w $@

build/%.h: %.h.sdoc
	mkdir -p build
	rm -f $@
	sdoc cat code.h::$< > $@
	chmod -w $@

build/xv-x64.c: build/xv-x64.h
build/xv-x64.h: build/xv.h

.PHONY: clean
clean:
	rm -rf build
