VERBOSE = @

TARGET_PATH = /opt/luci/ld-luci.so

SRCFOLDER = src
BUILDDIR ?= .build
LIBBEAN = bean/libbean.a
CXX = g++

CFLAGS ?=
CFLAGS += -ffunction-sections -fdata-sections -nostdlib
CFLAGS += -fno-jump-tables -fno-plt -fPIE
ifdef NO_FPU
CFLAGS += -mno-mmx -mno-sse -mgeneral-regs-only -DNO_FPU
endif
CFLAGS += -fno-builtin -fno-exceptions -fno-stack-protector -mno-red-zone

# Default Base address
BASEADDRESS = 0xba00000
# Default config file path
LIBPATH_CONF = /opt/luci/libpath.conf

CXXFLAGS ?= -O0 -g -std=c++2a $(CFLAGS)
CXXFLAGS += -I $(SRCFOLDER) -I $(dir $(LIBBEAN))/include/
# Elfo ELF class should be virtual & reference to DLH
CXXFLAGS += -DVIRTUAL -DUSE_DLH
# Disable several CXX features
CXXFLAGS += -fno-exceptions -fno-rtti -fno-use-cxa-atexit
CXXFLAGS += -nostdlib -nostdinc
CXXFLAGS += -Wall -Wextra -Wno-switch -Wno-nonnull-compare -Wno-unused-variable -Wno-comment
CXXFLAGS += -static-libgcc -DBASEADDRESS=$(BASEADDRESS) -DLIBPATH_CONF=$(LIBPATH_CONF) -DSONAME=$(notdir $(TARGET_PATH)) -DSOPATH=$(TARGET_PATH)
CXXFLAGS += -fvisibility=hidden

SOURCES = $(shell find $(SRCFOLDER)/ -name "*.cpp")
OBJECTS = $(patsubst $(SRCFOLDER)/%,$(BUILDDIR)/%,$(SOURCES:.cpp=.o))
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

$(TARGET_PATH): $(notdir $(TARGET_PATH))
	@echo "CP		$@"
	$(VERBOSE) cp $< $@

$(notdir $(TARGET_PATH)): $(OBJECTS) | $(LIBBEAN) $(BUILDDIR)
	@echo "LD		$@"
	$(VERBOSE) $(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) -L$(dir $(LIBBEAN)) -l$(patsubst lib%.a,%,$(notdir $(LIBBEAN))) -Wl,$(subst $(SPACE),$(COMMA),$(LDFLAGS))

$(BUILDDIR)/%.d : $(SRCFOLDER)/%.cpp $(MAKEFILE_LIST)
	@echo "DEP		$<"
	@mkdir -p $(@D)
	$(VERBOSE) $(CXX) $(CXXFLAGS) -MM -MP -MT $(BUILDDIR)/$*.o -MF $@ $<

$(BUILDDIR)/%.o : $(SRCFOLDER)/%.cpp $(MAKEFILE_LIST)
	@echo "CXX		$@"
	@mkdir -p $(@D)
	$(VERBOSE) $(CXX) $(CXXFLAGS) -D__MODULE__="Luci" -c -o $@ $<

$(LIBPATH_CONF): /etc/ld.so.conf gen-libpath.sh
	$(VERBOSE) ./gen-libpath.sh $< > $@

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

.PHONY: all clean mrproper
