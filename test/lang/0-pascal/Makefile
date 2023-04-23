DIR := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))
SETINTERP := $(DIR)/../../../bean/elfo/elfo-setinterp

OPTLEVEL ?= 2

FPC ?= fpc
FPCFLAGS += -O$(OPTLEVEL)

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
SOURCE = $(wildcard *.pas)

$(EXEC): $(SOURCE)
	$(FPC) $(FPCFLAGS) -o$@ $<
	test -n "$(LD_PATH)" && $(SETINTERP) $@ $(LD_PATH) || true
