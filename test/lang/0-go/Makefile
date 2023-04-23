GOBUILD = go build
GOFLAGS += -a

EXEC ?= run
SOURCE = $(wildcard *.go)
BIN = $(EXEC)-main

$(EXEC): $(BIN)
	@echo "#!/bin/sh" > $@
	@echo "$(LD_PATH) ./$<" >> $@
	@chmod +x $@

$(BIN): $(SOURCE)
	$(GOBUILD) $(GOFLAGS) -buildmode=exe -o $@ $^
