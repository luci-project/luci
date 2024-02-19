VERBOSE = @

NAME = luci

SRCFOLDER = src
OS ?= $(shell bash -c 'source /etc/os-release ; ID="$${ID/-/}" ; echo "$${ID,,}"')
OSVERSION ?= $(shell bash -c 'source /etc/os-release ; if [ -z "$${VERSION_CODENAME-}" ] ; then echo "$${VERSION_ID%%.*}" ; else echo "$${VERSION_CODENAME,,}" ; fi')
PLATFORM ?= x64
BUILDDIR ?= .build-$(OS)-$(OSVERSION)-$(PLATFORM)
LIBBEAN = bean/libbean.a
CXX = g++

CPPLINT ?= cpplint
CPPLINTIGNORE := bean example test
TIDY ?= clang-tidy
TIDYCONFIG ?= .clang-tidy


# Debian stretch only supports DWARF 4
DWARFVERSION := 4

# Compatibility macro name
COMPATIBILITY_PREFIX = COMPATIBILITY
COMPATIBILITY_MACROS = $(shell echo "-D$(COMPATIBILITY_PREFIX)_$(OS) -D$(COMPATIBILITY_PREFIX)_$(OS)_$(OSVERSION) -D$(COMPATIBILITY_PREFIX)_$(OS)_$(OSVERSION)_$(PLATFORM) -DPLATFORM_$(PLATFORM)" | tr a-z A-Z)

SOPATH = /opt/$(NAME)/
SONAME = ld-$(NAME).so
TARGET_FILE = ld-$(NAME)-$(OS)-$(OSVERSION)-$(PLATFORM).so

VERSIONS := versions.txt


# Default Luci base address
BASEADDRESS = 0x6ffff0000000
# Default library start address
LIBADDRESS = 0x600000000000
# Default position dependent address
PDCADDRESS = 0x400000
# Default config file path
LIBPATH_CONF = /opt/$(NAME)/libpath.conf
LDLUCI_CONF = /opt/$(NAME)/ld-$(NAME).conf
# Local patch offset file
PATCH_OFFSETS_LOCAL = src/comp/glibc/patch.offsets.local

ifeq ($(OPTIMIZE), 1)
	CXXFLAGS := -O3 -DNDEBUG
else
	CXXFLAGS := -Og -g -gdwarf-$(DWARFVERSION)
endif
CXXFLAGS += -std=c++2a
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
CXXFLAGS += -static-libgcc -DBASEADDRESS=$(BASEADDRESS)UL -DLIBADDRESS=$(LIBADDRESS)UL -DPDCADDRESS=$(PDCADDRESS)UL -DLIBPATH_CONF=$(LIBPATH_CONF) -DLDLUCI_CONF=$(LDLUCI_CONF) -DSONAME=$(SONAME) -DSOPATH=$(SOPATH)
CXXFLAGS += -fvisibility=hidden

BUILDINFO = $(BUILDDIR)/.build_$(NAME).o
SOURCES = $(shell find $(SRCFOLDER)/ -name "*.cpp")
OBJECTS = $(patsubst $(SRCFOLDER)/%,$(BUILDDIR)/%,$(SOURCES:.cpp=.o)) $(BUILDINFO)
DEPFILES = $(patsubst $(SRCFOLDER)/%,$(BUILDDIR)/%,$(SOURCES:.cpp=.d))
VERSION_SCRIPT = luci.version
EXPORT_SYMBOLS = $(shell cat $(VERSION_SCRIPT) | grep 'global:' | sed -e 's/global:\(.*\);/\1;/' | tr -d '\n;')
LDFLAGS = -pie -soname $(SONAME) --gc-sections -Ttext-segment=$(BASEADDRESS) --exclude-libs ALL --version-script=$(VERSION_SCRIPT) --no-dynamic-linker --export-dynamic -Bstatic $(addprefix --undefined=,$(EXPORT_SYMBOLS))


# Helper
SPACE = $(subst ,, )
COMMA = ,

define custom_version
	$(VERBOSE) /bin/echo -e "\e[1m$(1)ing luci for $(2) $(3) $(4)\e[0m"
	$(VERBOSE) $(MAKE) "OS=$(2)" "OSVERSION=$(3)" "PLATFORM=$(4)" "$(1)"
endef

define each_version
	$(VERBOSE) echo "$(1)ing luci for each version..."
	$(call custom_version,$(1),almalinux,9,x64)
	$(call custom_version,$(1),debian,stretch,x64)
	$(call custom_version,$(1),debian,buster,x64)
	$(call custom_version,$(1),debian,bullseye,x64)
	$(call custom_version,$(1),debian,bookworm,x64)
	$(call custom_version,$(1),fedora,36,x64)
	$(call custom_version,$(1),fedora,37,x64)
	$(call custom_version,$(1),ol,9,x64)
	$(call custom_version,$(1),opensuseleap,15,x64)
	$(call custom_version,$(1),rhel,9,x64)
	$(call custom_version,$(1),ubuntu,focal,x64)
	$(call custom_version,$(1),ubuntu,jammy,x64)
endef


install: $(SOPATH)$(SONAME) $(LIBPATH_CONF) $(LDLUCI_CONF)

install-only: $(LIBPATH_CONF) $(LDLUCI_CONF)
	$(VERBOSE) test -f "$(TARGET_FILE)"
	$(VERBOSE) ln -f -s $(shell readlink -f "$(TARGET_FILE)") "$(SOPATH)$(SONAME)"

build: $(TARGET_FILE)

all: $(VERSIONS)
	$(call each_version,build)

version-all:
	$(call each_version,version)

version:
	@echo "$(TARGET_FILE)"

$(VERSIONS): $(MAKEFILE_LIST)
	@echo "GEN		$@"
	@$(MAKE) version-all | egrep '^ld.*\.so$$' > $@

check:
	@if $(MAKE) version-all | grep '^$(TARGET_FILE)$$' >/dev/null ; then \
		echo "$(TARGET_FILE) is supported" ; \
	else \
		echo "$(TARGET_FILE) is not supported (yet)" ; \
		exit 1 ; \
	fi

test-all:
	$(call each_version,test)

test: $(TARGET_FILE)
	$(VERBOSE) uname -m | sed -e "s/^x86_64$$/x64/" | xargs test "$(PLATFORM)" =
	$(VERBOSE) ./tools/docker.sh $(OS):$(OSVERSION) /bin/sh -c "./test/run.sh && ./test/run.sh -u"

test-all-prepared:
	$(call each_version,test)

test-prepared: $(TARGET_FILE)
	$(VERBOSE) uname -m | sed -e "s/^x86_64$$/x64/" | xargs test "$(PLATFORM)" =
	$(VERBOSE) ./tools/docker.sh inf4/luci:$(OS)-$(OSVERSION) /bin/sh -c "./test/run.sh && ./test/run.sh -u"

$(PATCH_OFFSETS_LOCAL):
	@echo "GEN		$@"
	$(VERBOSE) tools/patch_offsets.sh > $@

$(LIBBEAN):
	@echo "GEN		$@"
	$(VERBOSE) $(MAKE) VERBOSE_MODE=0 DWARFVERSION=$(DWARFVERSION) OPTIMIZE=$(OPTIMIZE) -C $(@D)

$(SOPATH)$(TARGET_FILE): $(TARGET_FILE)
	@echo "CPY		$@"
	$(VERBOSE) cp $< $@

$(SOPATH)$(SONAME): $(SOPATH)$(TARGET_FILE)
	@echo "LINK		$@"
	$(VERBOSE) ln -f -r -s $< $@

$(TARGET_FILE): $(PATCH_OFFSETS_LOCAL) $(OBJECTS) | $(LIBBEAN) $(BUILDDIR)
	@echo "LD		$@"
	$(VERBOSE) $(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) -L$(dir $(LIBBEAN)) -Wl,--whole-archive -l$(patsubst lib%.a,%,$(notdir $(LIBBEAN))) -Wl,--no-whole-archive -Wl,$(subst $(SPACE),$(COMMA),$(LDFLAGS))
	$(VERBOSE) setcap cap_sys_ptrace=eip $@ >/dev/null 2>&1 && echo "CAP		$@" || true

$(BUILDDIR)/%.d : $(SRCFOLDER)/%.cpp $(MAKEFILE_LIST)
	@echo "DEP		$<"
	@mkdir -p $(@D)
	$(VERBOSE) $(CXX) $(CXXFLAGS) $(COMPATIBILITY_MACROS) -MM -MP -MT $(BUILDDIR)/$*.o -MF $@ $<

$(BUILDDIR)/%.o : $(SRCFOLDER)/%.cpp $(MAKEFILE_LIST)
	@echo "CXX		$@"
	@mkdir -p $(@D)
	$(VERBOSE) $(CXX) $(CXXFLAGS) -D__MODULE__="$(NAME)" $(COMPATIBILITY_MACROS) -c -o $@ $<

$(BUILDINFO): FORCE
	@echo "CXX		$@"
	@echo 'const char * build_$(NAME)_version() { return "$(shell git describe --dirty --always --tags 2>/dev/null || echo unknown)"; } ' \
	'const char * build_$(NAME)_date() { return "$(shell date -R)"; }' \
	'const char * build_$(NAME)_flags() { return "$(CXXFLAGS)"; }' \
	'const char * build_$(NAME)_compatibility() { return "$(OS) $(OSVERSION) on $(PLATFORM)"; }' | $(CXX) $(CXXFLAGS) -x c++ -c -o $@ -

$(LIBPATH_CONF): /etc/ld.so.conf gen-libpath.sh
	$(VERBOSE) mkdir -p $(dir $@)
	$(VERBOSE) ./gen-libpath.sh $< | grep -v "i386\|i486\|i686\|lib32\|libx32" > $@ || true

$(LDLUCI_CONF): example.conf
	$(VERBOSE) mkdir -p $(dir $@)
	$(VERBOSE) cp $< $@

lint::
	@if $(CPPLINT) --quiet --recursive $(addprefix --exclude=,$(CPPLINTIGNORE)) . ; then \
		echo "Congratulations, coding style obeyed!" ; \
	else \
		echo "Coding style violated -- see CPPLINT.cfg for details" ; \
		exit 1 ; \
	fi

tidy:: $(TIDYCONFIG)
	$(VERBOSE) $(TIDY) --config-file=$< --header-filter="^(?!.*(/capstone/|/test/))" --system-headers $(SOURCES) -- -stdlib=libc++ $(CXXFLAGS) -D__MODULE__="$(NAME)" $(COMPATIBILITY_MACROS)


clean::
	$(VERBOSE) rm -f $(DEPFILES) $(OBJECTS)
	$(VERBOSE) test -d $(BUILDDIR) && rm -rf $(BUILDDIR) || true

mrproper:: clean
	$(VERBOSE) rm -f $(SOPATH)$(SONAME) $(SOPATH)$(TARGET_FILE)

$(BUILDDIR): ; @mkdir -p $@

$(DEPFILES):

ifeq ($(filter-out all clean mrproper install-only,$(MAKECMDGOALS)),$(MAKECMDGOALS))
-include $(DEPFILES)
endif

FORCE:

.PHONY: test test-all test-prepared test-all-prepared install install-only all version version-all lint tidy clean mrproper
