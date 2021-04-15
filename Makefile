VERBOSE = @

CXX = g++
# TODO:-mno-sse  -mno-mmx -mgeneral-regs-only -fno-rtti -static-libstdc++
CXXFLAGS := -std=c++2a -pie -fno-exceptions -fno-rtti -Wall -Wno-comment -ffunction-sections -fdata-sections -Og -g -I src -I elfo/include -I plog/include

BUILDDIR ?= .build
CXX_SOURCES = $(wildcard src/*.cpp)
CXX_OBJECTS = $(addprefix $(BUILDDIR)/,$(CXX_SOURCES:.cpp=.o))
DEP_FILES = $(addprefix $(BUILDDIR)/,$(CXX_SOURCES:.cpp=.d) $(addsuffix .d,$(ASM_SOURCES)))
CXXFLAGS += $(addprefix -I ,$(dir $(LIBS)))
LDFLAGS = -L . $(addprefix -L ,$(dir $(LIBS))) -Wl,--gc-sections
TARGET_BIN = lilo
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
