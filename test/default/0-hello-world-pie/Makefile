OPTLEVEL ?= 2
CFLAGS += -O$(OPTLEVEL) -g -Wall -fPIC -pie

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
SOURCE = $(wildcard *.c)

$(EXEC): $(SOURCE)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
