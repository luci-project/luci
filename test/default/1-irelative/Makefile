CC ?= gcc
OPTLEVEL ?= 2
CFLAGS ?= -O$(OPTLEVEL) -g -Wall -Wno-unused-function -fPIC

ifdef LD_PATH
	CFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
SOURCE = main.c

$(EXEC): $(SOURCE)
	$(CC) $(CFLAGS) -o $@ $^
	readelf -r $@ | grep "_IRELATIV " > /dev/null
