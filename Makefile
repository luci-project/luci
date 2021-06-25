VERBOSE = @

SRCFOLDER = src
BUILDDIR ?= .build
LIBS := capstone dlh
INCLUDE := elfo bean

CXX = g++

CFLAGS ?= -Os -g
CFLAGS += -ffunction-sections -fdata-sections -nostdlib
ifdef NO_FPU
CFLAGS += -mno-mmx -mno-sse -mgeneral-regs-only -DNO_FPU
endif
CFLAGS += -fno-builtin -fno-exceptions -fno-stack-protector -fno-pic -mno-red-zone

CXXFLAGS ?= -std=c++2a $(CFLAGS)
CXXFLAGS += -I $(SRCFOLDER) $(foreach INC,$(LIBS) $(INCLUDE),-I $(INC)/include)
# Capstone includes depend on standard c includes
CXXFLAGS += -I dlh/legacy
# Elfo ELF class should be virtual & reference to DLH
CXXFLAGS += -DVIRTUAL -DUSE_DLH
# Disable several CXX features
CXXFLAGS += -fno-exceptions -fno-rtti -fno-use-cxa-atexit -no-pie
CXXFLAGS += -nostdlib -nostdinc
CXXFLAGS += -Wall -Wextra -Wno-switch -Wno-nonnull-compare -Wno-unused-variable -Wno-comment
CXXFLAGS += -static-libgcc

BUILDFLAGS_capstone := CFLAGS="$(CFLAGS) -Iinclude -DCAPSTONE_DIET -DCAPSTONE_X86_ATT_DISABLE -DCAPSTONE_HAS_X86" CAPSTONE_DIET=yes CAPSTONE_X86_ATT_DISABLE=yes CAPSTONE_ARCHS="x86" CAPSTONE_USE_SYS_DYN_MEM=yes CAPSTONE_STATIC=yes CAPSTONE_SHARED=no

BASEADDRESS = 0xbadc000

SOURCES = $(shell find $(SRCFOLDER)/ -name "*.cpp")
OBJECTS = $(patsubst $(SRCFOLDER)/%,$(BUILDDIR)/%,$(SOURCES:.cpp=.o))
DEPFILES = $(patsubst $(SRCFOLDER)/%,$(BUILDDIR)/%,$(SOURCES:.cpp=.d))
LDFLAGS = $(foreach LIB,$(LIBS),-L $(LIB)/ -l$(LIB)) -Wl,--gc-sections -Wl,-Ttext-segment=$(BASEADDRESS)
TARGET_BIN = luci
LIBPATH_CONF = libpath.conf


all: $(TARGET_BIN) $(LIBPATH_CONF)

$(TARGET_BIN): $(OBJECTS) | $(foreach LIB,$(LIBS),$(LIB)/lib$(LIB).a) $(BUILDDIR)
	@echo "LD		$@"
	$(VERBOSE) $(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

$(BUILDDIR)/%.d : $(SRCFOLDER)/%.cpp $(MAKEFILE_LIST)
	@echo "DEP		$<"
	@mkdir -p $(@D)
	$(VERBOSE) $(CXX) $(CXXFLAGS) -MM -MP -MT $(BUILDDIR)/$*.o -MF $@ $<

$(BUILDDIR)/%.o : $(SRCFOLDER)/%.cpp $(MAKEFILE_LIST)
	@echo "CXX		$@"
	@mkdir -p $(@D)
	$(VERBOSE) $(CXX) $(CXXFLAGS) -c -o $@ $<

define LIB_template =
$(1)/lib$(1).a:
	@echo "BUILD		$$@"
	@test -d $1 || git submodule update --init
	$$(VERBOSE) $$(MAKE) $$(BUILDFLAGS_$(1)) -j4 -C $1 $$(notdir $$@)

clean::
	@test -d $1 && $$(MAKE) -C $1 $$@

mrproper::
	@test -d $1 && $$(MAKE) -C $1 $$@ || true
	@rm -f $(1)/lib$(1).a
endef

$(foreach lib,$(LIBS),$(eval $(call LIB_template,$(lib))))

$(LIBPATH_CONF): /etc/ld.so.conf gen-libpath.sh
	$(VERBOSE) ./gen-libpath.sh $< > $@

clean::
	$(VERBOSE) rm -f $(DEPFILES)
	$(VERBOSE) test -d $(BUILDDIR) && rmdir $(BUILDDIR) || true

mrproper:: clean
	$(VERBOSE) rm -f $(TARGET_BIN)

$(BUILDDIR): ; @mkdir -p $@

$(DEPFILES):

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPFILES)
endif

.PHONY: all clean mrproper
