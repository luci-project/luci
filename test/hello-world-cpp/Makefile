LD_PATH ?= /lib64/ld-linux-x86-64.so.2

CXX ?= g++
CXXFLAGS ?= -O2 -g -Wall -Wl,--dynamic-linker=$(LD_PATH)

EXEC ?= run
SOURCE = hw.cpp


$(EXEC): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $@ $<
