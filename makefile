XV_CC_OPTS := -Wall -Wno-parentheses -Wno-unused-value -std=gnu99 -static
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

all: $(XV_TEST) $(XV_BIN) $(XV_DOC)
doc: $(XV_DOC)

debug: XV_CC_OPTS += -DXV_DEBUG_X64 -g
debug: $(XV_TEST) $(XV_BIN)

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

build/xv: build/xv.x $(XV_OBJ)
	$(LD) $(LD_OPTS) -T build/xv.x -o $@ $(XV_OBJ)

.PHONY: clean
clean:
	rm -rf build
