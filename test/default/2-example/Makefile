CFLAGS := -O$(OPTLEVEL) -gdwarf-4 -Wall -DDELAY=5

EXEC := run

test: $(EXEC)

include common.mk

$(EXEC): run-main
	@echo "#!/bin/bash" > $@
	@echo "for vers in $(LIBTARGET) ; do ln -f -s \$${vers} && echo "Using \$${vers}" >&2 ; sleep 5 ; done & " >> $@
	@echo "sleep 2" >> $@
	@echo "./$< $(words $(LIBTARGET))" >> $@
	@chmod +x $@

