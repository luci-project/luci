OPTLEVEL ?= 2
RUSTFLAGS += -C opt-level=$(OPTLEVEL) -g
RUSTC = rustc

ifdef LD_PATH
	LDFLAGS += -C link-args='-Wl,--dynamic-linker=$(LD_PATH)'
endif

EXEC ?= run
SOURCE = $(wildcard *.rs)

$(EXEC): $(SOURCE)
	$(RUSTC) $(RUSTFLAGS) $(LDFLAGS) -o $@ $^
