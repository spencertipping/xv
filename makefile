.SUFFIXES:

XV_CC_OPTS := -Wall -Wno-parentheses -Wno-unused-value -std=gnu99
XV_OBJ     := build/xv-x64.o build/xv.o
XV_BIN     := build/xv
XV_TEST    := test/disasm test/disasm.bin
XV_DOC     := $(subst .sdoc,.md,$(wildcard *.sdoc))
XV_HEADERS := $(patsubst %.sdoc,build/%,$(wildcard *.h.sdoc))
SDOC_EXTS  := c h x

CC         := gcc
LD         := ld

all: $(XV_DOC) real
doc: $(XV_DOC)

define sdoc_unwrap
build/%.$(1): %.$(1).sdoc
	mkdir -p build
	rm -f $$@
	sdoc cat code.$(1)::$$< > $$@
	chmod -w $$@
endef

$(foreach ext,$(SDOC_EXTS),$(eval $(call sdoc_unwrap,$(ext))))

# TODO: fix horrible makefile
.PHONY: debug real
debug: XV_CC_OPTS += -DXV_DEBUG_X64 -g
debug: XV_LD_OPTS += -lc
debug: $(XV_TEST)

real: XV_CC_OPTS += -nostdlib -ffreestanding
real: XV_LD_OPTS += -T build/xv.x
real: $(XV_BIN)

test/%: test/%.c $(XV_OBJ)
	$(CC) $(CC_OPTS) $(XV_CC_OPTS) $< $(XV_OBJ) -o $@

# This gives us some flat machine code on the host platform to test with.
test/%.bin: test/%.o
	objcopy -O binary -j .text $< $@

%.o: %.c makefile $(XV_HEADERS)
	$(CC) $(CC_OPTS) $(XV_CC_OPTS) $< -c -o $@

%.md: %.sdoc
	sdoc cat markdown::$< > $@

build/xv-x64.c: build/xv-x64.h
build/xv-x64.h: build/xv.h

build/xv: build/xv.x $(XV_OBJ)
	$(LD) $(LD_OPTS) $(XV_LD_OPTS) -o $@ $(XV_OBJ)

.PHONY: clean
clean:
	rm -rf build $(subst .c,,$(wildcard test/*.c)) \
		     $(subst .c,.o,$(wildcard test/*.c)) \
		     $(subst .c,.bin,$(wildcard test/*.c))
