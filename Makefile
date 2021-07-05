VERBOSE = @

SRCFOLDER = src
BUILDDIR ?= .build
LIBS := capstone dlh
INCLUDE := elfo bean

CXX = g++

CFLAGS ?= -Og -g
CFLAGS += -ffunction-sections -fdata-sections -nostdlib
CFLAGS += -fno-jump-tables -fno-plt -fPIE
ifdef NO_FPU
CFLAGS += -mno-mmx -mno-sse -mgeneral-regs-only -DNO_FPU
endif
CFLAGS += -fno-builtin -fno-exceptions -fno-stack-protector -mno-red-zone

# Default Base address
BASEADDRESS = 0xbadc000
# Default config file path
LIBPATH_CONF = /opt/luci/libpath.conf

CXXFLAGS ?= -std=c++2a $(CFLAGS)
CXXFLAGS += -I $(SRCFOLDER) $(foreach INC,$(LIBS) $(INCLUDE),-I $(INC)/include)
# Capstone includes depend on standard c includes
CXXFLAGS += -I dlh/legacy
# Elfo ELF class should be virtual & reference to DLH
CXXFLAGS += -DVIRTUAL -DUSE_DLH
# Disable several CXX features
CXXFLAGS += -fno-exceptions -fno-rtti -fno-use-cxa-atexit
CXXFLAGS += -nostdlib -nostdinc
CXXFLAGS += -Wall -Wextra -Wno-switch -Wno-nonnull-compare -Wno-unused-variable -Wno-comment
CXXFLAGS += -static-libgcc -DBASEADDRESS=$(BASEADDRESS) -DLIBPATH_CONF=$(LIBPATH_CONF)
CXXFLAGS += -fvisibility=hidden

BUILDFLAGS_capstone := CFLAGS="$(CFLAGS) -Iinclude -DCAPSTONE_DIET -DCAPSTONE_X86_ATT_DISABLE -DCAPSTONE_HAS_X86" CAPSTONE_DIET=yes CAPSTONE_X86_ATT_DISABLE=yes CAPSTONE_ARCHS="x86" CAPSTONE_USE_SYS_DYN_MEM=yes CAPSTONE_STATIC=yes CAPSTONE_SHARED=yes

TARGET_BIN = luci
SOURCES = $(shell find $(SRCFOLDER)/ -name "*.cpp")
OBJECTS = $(patsubst $(SRCFOLDER)/%,$(BUILDDIR)/%,$(SOURCES:.cpp=.o))
DEPFILES = $(patsubst $(SRCFOLDER)/%,$(BUILDDIR)/%,$(SOURCES:.cpp=.d))
VERSION_SCRIPT = luci.version
EXPORT_SYMBOLS = $(shell cat $(VERSION_SCRIPT) | grep 'global:' | sed -e 's/global:\(.*\);/\1;/' | tr -d '\n;')
LDFLAGS = -pie -soname test --gc-sections -Ttext-segment=$(BASEADDRESS) --exclude-libs ALL --version-script=$(VERSION_SCRIPT) --no-dynamic-linker --export-dynamic -Bstatic $(addprefix --undefined=,$(EXPORT_SYMBOLS))


# Helper
SPACE = $(subst ,, )
COMMA = ,

all: $(TARGET_BIN) $(LIBPATH_CONF)

$(TARGET_BIN): $(OBJECTS) | $(foreach LIB,$(LIBS),$(LIB)/lib$(LIB).a) $(BUILDDIR)
	@echo "LD		$@"
	$(VERBOSE) $(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) $(foreach LIB,$(LIBS),-L $(LIB)/ -l$(LIB)) -Wl,$(subst $(SPACE),$(COMMA),$(LDFLAGS))

$(BUILDDIR)/%.d : $(SRCFOLDER)/%.cpp $(MAKEFILE_LIST)
	@echo "DEP		$<"
	@mkdir -p $(@D)
	$(VERBOSE) $(CXX) $(CXXFLAGS) -MM -MP -MT $(BUILDDIR)/$*.o -MF $@ $<

$(BUILDDIR)/%.o : $(SRCFOLDER)/%.cpp $(MAKEFILE_LIST)
	@echo "CXX		$@"
	@mkdir -p $(@D)
	$(VERBOSE) $(CXX) $(CXXFLAGS) -c -o $@ $<

define LIB_template =
$(1)/lib$(1).a: $(MAKEFILE_LIST)
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
