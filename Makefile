VERBOSE = @

CXX = g++
CXXFLAGS := -std=c++2a -pie -pedantic -Wall -Wno-comment -Og -g -I src -I ELFIO -I cxxopts/include -I zydis/include -I zydis/dependencies/zycore/include -I plog/include

BUILDDIR ?= .build
CXX_SOURCES = $(wildcard src/*.cpp)
CXX_OBJECTS = $(addprefix $(BUILDDIR)/,$(CXX_SOURCES:.cpp=.o))
DEP_FILES = $(addprefix $(BUILDDIR)/,$(CXX_SOURCES:.cpp=.d) $(addsuffix .d,$(ASM_SOURCES)))
LIBS = zydis/build/libZydis.a zydis/dependencies/zycore/build/libZycore.a
CXXFLAGS += $(addprefix -I ,$(dir $(LIBS)))
LDFLAGS = -L . $(addprefix -L ,$(dir $(LIBS))) -lZydis -lZycore
TARGET_BIN = lilo


all: $(TARGET_BIN)

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

clean:
	@echo "RM		$(BUILDDIR)"
	$(VERBOSE) rm -rf $(BUILDDIR)


ifneq ($(MAKECMDGOALS),clean)
-include $(DEP_FILES)
endif

# Phony targets
.PHONY: all clean
