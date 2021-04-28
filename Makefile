VERBOSE = @

INCLUDE = src elfo/include bean/include bean/capstone/include bean/xxhash plog/include

CXX = g++
#CXXFLAGS := -std=c++2a -fno-exceptions -fno-rtti -Wall -static-libstdc++ -static-libgcc -static -Wno-comment -Og -g
CXXFLAGS := -std=c++2a -fno-exceptions -fno-rtti -Wall -Wno-comment -Og -g

BASEADDRESS = 0xbadc000

BUILDDIR ?= .build
CXX_SOURCES = $(wildcard src/*.cpp)
CXX_OBJECTS = $(addprefix $(BUILDDIR)/,$(CXX_SOURCES:.cpp=.o))
DEP_FILES = $(addprefix $(BUILDDIR)/,$(CXX_SOURCES:.cpp=.d) $(addsuffix .d,$(ASM_SOURCES)))
CXXFLAGS += $(addprefix -I , $(INCLUDE)) $(addprefix -I ,$(dir $(LIBS)))
LDFLAGS = -L . $(addprefix -L ,$(dir $(LIBS))) -Wl,-Ttext-segment=$(BASEADDRESS)
TARGET_BIN = luci
LIBPATH_CONF = libpath.conf


all: $(TARGET_BIN) $(LIBPATH_CONF)

%.a:
	mkdir -p $(dir $@)
	cd $(dir $@) && cmake ..
	make -C $(dir $@)

$(TARGET_BIN): $(CXX_OBJECTS) $(LIBS)
	$(VERBOSE) $(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

$(BUILDDIR)/%.d : %.cpp $(MAKEFILE_LIST)
	@echo "DEP		$<"
	@mkdir -p $(@D)
	$(VERBOSE) $(CXX) $(CXXFLAGS) -MM -MT $(BUILDDIR)/$*.o -MF $@ $<

$(BUILDDIR)/%.o : %.cpp $(MAKEFILE_LIST)
	@echo "CXX		$<"
	@mkdir -p $(@D)
	$(VERBOSE) $(CXX) $(CXXFLAGS) -c -o $@ $<

$(LIBPATH_CONF): /etc/ld.so.conf gen-libpath.sh
	$(VERBOSE) ./gen-libpath.sh $< > $@

clean:
	@echo "RM		$(BUILDDIR)"
	$(VERBOSE) rm -rf $(BUILDDIR)


ifneq ($(MAKECMDGOALS),clean)
-include $(DEP_FILES)
endif

# Phony targets
.PHONY: all clean
