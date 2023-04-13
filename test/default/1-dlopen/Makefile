OPTLEVEL ?= 2
CFLAGS += -O$(OPTLEVEL) -g -Wall
LDFLAGS = -ldl
ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
SOURCE = $(wildcard *.c)

$(EXEC): $(SOURCE)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
