OPTLEVEL ?= 2
CFLAGS += -O$(OPTLEVEL) -z pack-relative-relocs

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH) -lpthread
endif

EXEC ?= run
SOURCE = $(wildcard *.c)

$(EXEC): $(SOURCE)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
