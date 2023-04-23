OPTLEVEL ?= 2
FFLAGS += -O$(OPTLEVEL) -g -Wall
FC := gfortran

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
SOURCE = $(wildcard *.f90)

$(EXEC): $(SOURCE)
	$(FC) $(FFLAGS) $(LDFLAGS) -o $@ $^
