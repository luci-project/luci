VERBOSE = @

BUILDDIR ?= .build
SOURCE_FOLDER = src
CXX = g++
CXXFLAGS = -std=c++2a -fno-exceptions -fno-rtti -static-libgcc -Wall -Wno-nonnull-compare -Wno-comment -Og -g -I include
SOURCES = $(shell find $(SOURCE_FOLDER)/ -name "*.cpp")
OBJECTS = $(patsubst $(SOURCE_FOLDER)/%,$(BUILDDIR)/%,$(SOURCES:.cpp=.o))
DEPFILES = $(patsubst $(SOURCE_FOLDER)/%,$(BUILDDIR)/%,$(SOURCES:.cpp=.d))
TARGET = libdlh.a

all: $(TARGET)

%.a: $(OBJECTS)
	ar rcs $@ $^

$(BUILDDIR)/%.d: $(SOURCE_FOLDER)/%.cpp $(MAKEFILE_LIST)
	@echo "DEP		$<"
	@mkdir -p $(@D)
	$(VERBOSE) $(CXX) $(CXXFLAGS) -MM -MT $(BUILDDIR)/$*.o -MF $@ $<

$(BUILDDIR)/%.o: $(SOURCE_FOLDER)/%.cpp $(MAKEFILE_LIST)
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
