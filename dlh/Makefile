DEPDIR := .deps
CXXFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$@.d -Og -g -I include/
SOURCES := $(wildcard src/*.cpp)
TARGETS := $(notdir $(SOURCES:%.cpp=%))
DEPFILES := $(addprefix $(DEPDIR)/,$(addsuffix .d,$(TARGETS)))

all: $(TARGETS)

%: src/%.cpp | $(DEPDIR)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(DEPDIR): ; @mkdir -p $@

$(DEPFILES):

clean::
	rm -f $(DEPFILES)
	rmdir $(DEPDIR) || true

include $(wildcard $(DEPFILES))
