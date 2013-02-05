%.h: %.h.sdoc
	sdoc cat code.h::$< > $@

%.c: %.c.sdoc
	sdoc cat code.c::$< > $@
