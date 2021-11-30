OPTLEVEL ?= 2
CXXFLAGS ?= -O$(OPTLEVEL) -g -Wall -fPIC

EXEC ?= run
LIBS ?= extern

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

$(EXEC): main.o $(addsuffix .so,$(addprefix lib,$(LIBS)))
	$(CXX) $(LDFLAGS) -o $@ $< -L. $(addprefix -l,$(LIBS))

%.o: %.c
	$(CXX) $(CFLAGS) -c -o $@ $<

lib%.so: %.o
	$(CXX) -shared -o $@ $<
