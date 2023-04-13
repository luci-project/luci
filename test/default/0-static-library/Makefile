OPTLEVEL ?= 2
CFLAGS += -O$(OPTLEVEL) -g -Wall

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
LIBS ?= extern nested

$(EXEC): main.o $(addsuffix .a,$(addprefix lib,$(LIBS)))
	$(CC) $(LDFLAGS) -o $@ $< -L. $(addprefix -l,$(LIBS))

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

lib%.a: %.o
	ar rcs $@ $<
