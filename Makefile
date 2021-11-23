VERBOSE = @

NAME = luci

SRCFOLDER = src
OS ?= $(shell bash -c 'source /etc/os-release ; echo "$${ID,,}"')
OSVERSION ?= $(shell bash -c 'source /etc/os-release ; echo "$${VERSION_CODENAME,,}"')
PLATFORM ?= x64
BUILDDIR ?= .build-$(OS)-$(OSVERSION)-$(PLATFORM)
LIBBEAN = bean/libbean.a
CXX = g++

# Compatibility macro name
COMPATIBILITY_PREFIX = COMPATIBILITY
COMPATIBILITY = $(shell echo "$(COMPATIBILITY_PREFIX)_$(OS)_$(OSVERSION)_$(PLATFORM)" | tr a-z A-Z)

TARGET_FILE = ld-$(NAME)-$(OS)-$(OSVERSION)-$(PLATFORM).so
TARGET_PATH = /opt/$(NAME)/$(TARGET_FILE)

CRFLAGS ?=


# Default Luci base address
BASEADDRESS = 0x6ffff0000000
# Default library start address
LIBADDRESS = 0x600000000000
# Default config file path
LIBPATH_CONF = /opt/$(NAME)/libpath.conf

CXXFLAGS ?= -Og -g -std=c++2a
CXXFLAGS += -I $(SRCFOLDER) -I $(dir $(LIBBEAN))/include/
# Elfo ELF class should be virtual & reference to DLH
CXXFLAGS += -DVIRTUAL -DUSE_DLH
# Disable FPU?
ifdef NO_FPU
CXXFLAGS += -mno-mmx -mno-sse -mgeneral-regs-only -DNO_FPU
endif
# Disable several CXX features
CXXFLAGS += -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-jump-tables -fno-plt -fPIE
CXXFLAGS += -fno-builtin -fno-exceptions -fno-stack-protector -mno-red-zone
CXXFLAGS += -ffreestanding -ffunction-sections -fdata-sections -nostdlib -nostdinc
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

install: $(TARGET_PATH) $(LIBPATH_CONF)

build: $(TARGET_FILE)

all:
	@grep -r '$(COMPATIBILITY_PREFIX)' $(SRCFOLDER) | sed -ne 's/^.*defined($(COMPATIBILITY_PREFIX)_\([^_]*\)_\([^_]*\)_\([^)]*\)).*$$/\L\1 \2 \3/p' | sort -u | xargs -n3 sh -c 'echo "\e[1mBuilding luci for $$0 $$1 ($$2)\e[0m" ; $(MAKE) OS=$$0 OSVERSION=$$1 PLATFORM=$$2 build'

$(LIBBEAN):
	@echo "GEN		$@"
	$(VERBOSE) $(MAKE) DIET=1 -C $(@D)

$(TARGET_PATH): $(TARGET_FILE)
	@echo "CP		$@"
	$(VERBOSE) cp $< $@

$(TARGET_FILE): $(OBJECTS) | $(LIBBEAN) $(BUILDDIR)
	@echo "LD		$@"
	$(VERBOSE) $(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) -L$(dir $(LIBBEAN)) -Wl,--whole-archive -l$(patsubst lib%.a,%,$(notdir $(LIBBEAN))) -Wl,--no-whole-archive -Wl,$(subst $(SPACE),$(COMMA),$(LDFLAGS))

$(BUILDDIR)/%.d : $(SRCFOLDER)/%.cpp $(MAKEFILE_LIST)
	@echo "DEP		$<"
	@mkdir -p $(@D)
	$(VERBOSE) $(CXX) $(CXXFLAGS) -D$(COMPATIBILITY) -MM -MP -MT $(BUILDDIR)/$*.o -MF $@ $<

$(BUILDDIR)/%.o : $(SRCFOLDER)/%.cpp $(MAKEFILE_LIST)
	@echo "CXX		$@"
	@mkdir -p $(@D)
	$(VERBOSE) $(CXX) $(CXXFLAGS) -D__MODULE__="$(NAME)" -D$(COMPATIBILITY) -c -o $@ $<

$(BUILDINFO): FORCE
	@echo "CXX		$@"
	@echo 'const char * build_$(NAME)_version() { return "$(shell git describe --dirty --always --tags)"; } ' \
	'const char * build_$(NAME)_date() { return "$(shell date -R)"; }' \
	'const char * build_$(NAME)_flags() { return "$(CXXFLAGS)"; }' \
	'const char * build_$(NAME)_compatibility() { return "$(OS) $(OSVERSION) on $(PLATFORM) (macro $(COMPATIBILITY))"; }' | $(CXX) $(CXXFLAGS) -x c++ -c -o $@ -

$(LIBPATH_CONF): /etc/ld.so.conf gen-libpath.sh
	$(VERBOSE) ./gen-libpath.sh $< | grep -v "i386\|i486\|i686\|lib32\|libx32" > $@

clean::
	$(VERBOSE) rm -f $(DEPFILES) $(OBJECTS)
	$(VERBOSE) test -d $(BUILDDIR) && rm -rf $(BUILDDIR) || true

mrproper:: clean
	$(VERBOSE) rm -f $(notdir $(TARGET_PATH))

$(BUILDDIR): ; @mkdir -p $@

$(DEPFILES):

ifeq ($(filter-out all clean,$(MAKECMDGOALS)),$(MAKECMDGOALS))
-include $(DEPFILES)
endif

FORCE:

.PHONY: install all clean mrproper
