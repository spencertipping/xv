XV_CC_OPTS := -Wall -Wno-parentheses -Wno-unused-value -std=gnu99
XV_OBJ     := build/xv-x64.o
XV_BIN     := build/xv
XV_TEST    := test/disasm test/disasm.bin
XV_DOC     := $(subst .sdoc,.md,$(wildcard *.sdoc))
SDOC_EXTS  := c h x

define sdoc_unwrap
build/%.$(1): %.$(1).sdoc
	mkdir -p build
	rm -f $$@
	sdoc cat code.$(1)::$$< > $$@
	chmod -w $$@
endef

all: $(XV_DOC) real
doc: $(XV_DOC)

.PHONY: debug real
debug: XV_CC_OPTS += -DXV_DEBUG_X64 -g
debug: $(XV_TEST) $(XV_BIN)

real: XV_CC_OPTS += -nostdlib
real: $(XV_PRODTEST) $(XV_BIN)

test/%: test/%.c $(XV_OBJ)
	$(CC) $(CC_OPTS) $(XV_CC_OPTS) $< $(XV_OBJ) -o $@

test/%.bin: test/%.o
	objcopy -O binary -j .text $< $@

%.o: %.c makefile
	$(CC) $(CC_OPTS) $(XV_CC_OPTS) $< -c -o $@

%.md: %.sdoc
	sdoc cat markdown::$< > $@

$(foreach ext,$(SDOC_EXTS),$(eval $(call sdoc_unwrap,$(ext))))

build/xv-x64.c: build/xv-x64.h
build/xv-x64.h: build/xv.h

build/xv: build/xv.x build/xv.o $(XV_OBJ)
	$(LD) $(LD_OPTS) -T build/xv.x -o $@ build/xv.o

.PHONY: clean
clean:
	rm -rf build
