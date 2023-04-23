DIR := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))
SETINTERP := $(DIR)/../../../bean/elfo/elfo-setinterp

OPTLEVEL ?= 2

FPC ?= fpc
FPCFLAGS += -O$(OPTLEVEL) -Tlinux -XD -Cg

ifdef LD_PATH
	FPCFLAGS += -FL$(LD_PATH)
endif

EXEC ?= run
SOURCE = $(wildcard *.pas)

$(EXEC): $(SOURCE)
	$(FPC) $(FPCFLAGS) -o$@ $<