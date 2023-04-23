OPTLEVEL ?= 2

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

GOBUILD = go build
GOFLAGS += -a -compiler gccgo -gccgoflags '-O$(OPTLEVEL) -g -Wall $(LDFLAGS)'

EXEC ?= run
SOURCE = $(wildcard *.go)

$(EXEC): $(SOURCE)
	$(GOBUILD) $(GOFLAGS) -buildmode=exe -o $@ $^
