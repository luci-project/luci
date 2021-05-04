VERBOSE = @

INCLUDE = src elfo/include bean/include capstone/include xxhash plog/include
LIBCAPSTONE = capstone/libcapstone.a

CXX = g++
#CXXFLAGS := -std=c++2a -fno-exceptions -fno-rtti -Wall -static-libstdc++ -static-libgcc -static -Wno-comment -Og -g
CXXFLAGS := -std=c++2a -fno-exceptions -fno-rtti -Wall -Wno-comment -Og -g -pthread

BASEADDRESS = 0xbadc000

BUILDDIR ?= .build
CXX_SOURCES = $(wildcard src/*.cpp)
CXX_OBJECTS = $(addprefix $(BUILDDIR)/,$(CXX_SOURCES:.cpp=.o))
DEP_FILES = $(addprefix $(BUILDDIR)/,$(CXX_SOURCES:.cpp=.d) $(addsuffix .d,$(ASM_SOURCES)))
CXXFLAGS += $(addprefix -I , $(INCLUDE))
LDFLAGS = -L . -lcapstone -Lcapstone -Wl,-Ttext-segment=$(BASEADDRESS)
TARGET_BIN = luci
LIBPATH_CONF = libpath.conf


all: $(TARGET_BIN) $(LIBPATH_CONF)

$(TARGET_BIN): $(CXX_OBJECTS) $(LIBCAPSTONE)
	$(VERBOSE) $(CXX) $(CXXFLAGS) -o $@ $(CXX_OBJECTS) $(LDFLAGS)

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

$(LIBCAPSTONE):
	@echo "BUILD		$<"
	git submodule update --init
	$(MAKE) CAPSTONE_DIET=yes CAPSTONE_ARCHS="x86" -C capstone -j 4

clean:
	@echo "RM		$(BUILDDIR)"
	$(VERBOSE) rm -rf $(BUILDDIR)


ifneq ($(MAKECMDGOALS),clean)
-include $(DEP_FILES)
endif

# Phony targets
.PHONY: all clean
