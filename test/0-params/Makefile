OPTLEVEL ?= 2
CFLAGS += -O$(OPTLEVEL) -g -Wall

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
BIN = $(EXEC)-main
SOURCE = $(wildcard *.c)

$(EXEC): $(BIN)
	@echo "#!/bin/sh" > $@
	@echo "export PARAM_TEST_NAME=Pneumonoultramicroscopicsilicovolcanoconiosis" >> $@
	@echo "./$< -f oo  --bar 23 \"Baz !\" ? " >> $@
	@chmod +x $@

$(BIN): $(SOURCE)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
