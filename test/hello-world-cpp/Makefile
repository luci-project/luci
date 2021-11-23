RTLD ?= /lib64/ld-linux-x86-64.so.2

CXX ?= g++
CXXFLAGS ?= -O2 -g -Wall -Wl,--dynamic-linker=$(RTLD)

BINARY ?= run
SOURCE = hw.cpp


$(BINARY): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $@ $<


