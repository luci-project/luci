OPTLEVEL ?= 2
CFLAGS += -O$(OPTLEVEL) -g -Wall -fPIC

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
LIBS ?= extern nested

$(EXEC): main.o $(addsuffix .so,$(addprefix lib,$(LIBS)))
	$(CC) $(LDFLAGS) -o $@ $< -L. $(addprefix -l,$(LIBS))

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

lib%.so: %.o
	$(CC) -shared -o $@ $<
