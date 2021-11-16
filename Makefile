VERBOSE = @

NAME = luci

TARGET_PATH = /opt/$(NAME)/ld-$(NAME).so

SRCFOLDER = src
BUILDDIR ?= .build
COMPATIBILITY ?= $(shell bash -c 'source /etc/os-release ; echo "$${ID^^}_$${VERSION_CODENAME^^}"')
PLATFORM ?= AMD64
LIBBEAN = bean/libbean.a
CXX = g++

CFLAGS ?=
CFLAGS += -ffreestanding -ffunction-sections -fdata-sections -nostdlib
CFLAGS += -fno-jump-tables -fno-plt -fPIE
ifdef NO_FPU
CFLAGS += -mno-mmx -mno-sse -mgeneral-regs-only -DNO_FPU
endif
CFLAGS += -fno-builtin -fno-exceptions -fno-stack-protector -mno-red-zone
CFLAGS += -DCOMPATIBILITY_$(COMPATIBILITY)_$(PLATFORM)

# Default Luci base address
BASEADDRESS = 0x6ffff0000000
# Default library start address
LIBADDRESS = 0x600000000000
# Default config file path
LIBPATH_CONF = /opt/$(NAME)/libpath.conf

CXXFLAGS ?= -O0 -g -std=c++2a $(CFLAGS)
CXXFLAGS += -I $(SRCFOLDER) -I $(dir $(LIBBEAN))/include/
# Elfo ELF class should be virtual & reference to DLH
CXXFLAGS += -DVIRTUAL -DUSE_DLH
# Disable several CXX features
CXXFLAGS += -fno-exceptions -fno-rtti -fno-use-cxa-atexit
CXXFLAGS += -nostdlib -nostdinc
CXXFLAGS += -Wall -Wextra -Wno-switch -Wno-nonnull-compare -Wno-unused-variable -Wno-comment
CXXFLAGS += -static-libgcc -DBASEADDRESS=$(BASEADDRESS)UL -DLIBADDRESS=$(LIBADDRESS)UL -DLIBPATH_CONF=$(LIBPATH_CONF) -DSONAME=$(notdir $(TARGET_PATH)) -DSOPATH=$(TARGET_PATH)
CXXFLAGS += -fvisibility=hidden

BUILDINFO = $(BUILDDIR)/.build_$(NAME).o
SOURCES = $(shell find $(SRCFOLDER)/ -name "*.cpp")
OBJECTS = $(patsubst $(SRCFOLDER)/%,$(BUILDDIR)/%,$(SOURCES:.cpp=.o)) $(BUILDINFO)
DEPFILES = $(patsubst $(SRCFOLDER)/%,$(BUILDDIR)/%,$(SOURCES:.cpp=.d))
VERSION_SCRIPT = luci.version
EXPORT_SYMBOLS = $(shell cat $(VERSION_SCRIPT) | grep 'global:' | sed -e 's/global:\(.*\);/\1;/' | tr -d '\n;')
LDFLAGS = -pie -soname $(notdir $(TARGET_PATH)) --gc-sections -Ttext-segment=$(BASEADDRESS) --exclude-libs ALL --version-script=$(VERSION_SCRIPT) --no-dynamic-linker --export-dynamic -Bstatic $(addprefix --undefined=,$(EXPORT_SYMBOLS))


# Helper
SPACE = $(subst ,, )
COMMA = ,

all: $(TARGET_PATH) $(LIBPATH_CONF)

$(LIBBEAN):
	@echo "GEN		$@"
	$(VERBOSE) $(MAKE) DIET=1 -C $(@D)
	#$(VERBOSE) $(MAKE) -C $(@D)

$(TARGET_PATH): $(notdir $(TARGET_PATH))
	@echo "CP		$@"
	$(VERBOSE) cp $< $@

$(notdir $(TARGET_PATH)): $(OBJECTS) | $(LIBBEAN) $(BUILDDIR)
	@echo "LD		$@"
	$(VERBOSE) $(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) -L$(dir $(LIBBEAN)) -Wl,--whole-archive -l$(patsubst lib%.a,%,$(notdir $(LIBBEAN))) -Wl,--no-whole-archive -Wl,$(subst $(SPACE),$(COMMA),$(LDFLAGS))

$(BUILDDIR)/%.d : $(SRCFOLDER)/%.cpp $(MAKEFILE_LIST)
	@echo "DEP		$<"
	@mkdir -p $(@D)
	$(VERBOSE) $(CXX) $(CXXFLAGS) -MM -MP -MT $(BUILDDIR)/$*.o -MF $@ $<

$(BUILDDIR)/%.o : $(SRCFOLDER)/%.cpp $(MAKEFILE_LIST)
	@echo "CXX		$@"
	@mkdir -p $(@D)
	$(VERBOSE) $(CXX) $(CXXFLAGS) -D__MODULE__="$(NAME)" -c -o $@ $<

$(BUILDINFO): FORCE
	@echo "CXX		$@"
	@echo 'const char * build_$(NAME)_version() { return "$(shell git describe --dirty --always --tags)"; } ' \
	'const char * build_$(NAME)_date() { return "$(shell date -R)"; }' \
	'const char * build_$(NAME)_flags() { return "$(CXXFLAGS)"; }' | $(CXX) $(CXXFLAGS) -x c++ -c -o $@ -

$(LIBPATH_CONF): /etc/ld.so.conf gen-libpath.sh
	$(VERBOSE) ./gen-libpath.sh $< | grep -v "i386\|i486\|i686\|lib32\|libx32" > $@

clean::
	$(VERBOSE) rm -f $(DEPFILES)
	$(VERBOSE) test -d $(BUILDDIR) && rmdir $(BUILDDIR) || true

mrproper:: clean
	$(VERBOSE) rm -f $(notdir $(TARGET_PATH))

$(BUILDDIR): ; @mkdir -p $@

$(DEPFILES):

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPFILES)
endif

FORCE:

.PHONY: all clean mrproper
