OPTLEVEL ?= 2
CXXFLAGS += -O$(OPTLEVEL) -g -Wall

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
SOURCE = $(wildcard *.cpp)

$(EXEC): $(SOURCE)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^
