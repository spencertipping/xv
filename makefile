XV_CC_OPTS = -Wall -Wno-parentheses -std=gnu99
XV_OBJ     = build/xv-x64.o
XV_TEST    = test/disasm test/disasm.bin

all: $(XV_TEST)

test/%: test/%.c $(XV_OBJ)
	$(CC) $(CC_OPTS) $(XV_CC_OPTS) $< $(XV_OBJ) -o $@

test/%.bin: test/%.o
	objcopy -O binary -j .text $< $@

%.o: %.c makefile
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
