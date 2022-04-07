DIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
EXEC ?= run
BIN = $(EXEC)-bash

$(EXEC): $(BIN) $(MAKEFILE_LIST)
	@echo "#!/bin/sh" > $@
	@echo "exec ./$< -x -c 'for i in \$$(seq 1 11) ; do echo \"\$$(( 11 - i ))...\" ; done ; ( echo Fire ; sleep 1 ) & wait \$$! && echo -e \"  __\\\\n  \\\\\\\\ \\\\\\\\_____\\\\n##[==_____>\\\\n  /_/\"'" >> $@
	@chmod +x $@

$(BIN): /bin/bash
	cp $< $@
	if [ -n "$(LD_PATH)" ] ; then $(DIR)/../../bean/elfo/elfo-setinterp $@ $(LD_PATH) ; fi

FORCE:
