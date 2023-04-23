DIR := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))
SETINTERP := $(DIR)/../../../bean/elfo/elfo-setinterp

GOBUILD = go build
GOFLAGS += -a

EXEC ?= run
SOURCE = $(wildcard *.go)

$(EXEC): $(SOURCE)
	$(GOBUILD) $(GOFLAGS) -buildmode=pie -o $@ $^
	test -n "$(LD_PATH)" && $(SETINTERP) $@ $(LD_PATH) || true
