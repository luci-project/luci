OPTLEVEL ?= 2

GNATMAKE = gnatmake 
GNATFLAGS = -f -O$(OPTLEVEL) -g

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
SOURCE = $(filter-out b~%,$(wildcard *.adb))

$(EXEC): $(SOURCE)
	$(GNATMAKE) $(GNATFLAGS) -o $@ $(patsubst %.adb,%,$<) -largs="$(LDFLAGS)"
