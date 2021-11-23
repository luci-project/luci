all: test-static test-so test-pie test-go test-go-pie

test-static:
	make -f Makefile.STATIC clean
	make -f Makefile.STATIC $@

test-so:
	make -f Makefile.SO clean
	make -f Makefile.SO $@

test-pie:
	make -f Makefile.PIE clean
	make -f Makefile.PIE $@

test-go:
	make -f Makefile.GO $@

test-go-pie:
	make -f Makefile.GO $@

clean:
	make -f Makefile.SO clean
	make -f Makefile.PIE clean

.PHONY: test-static test-so test-pie test-go test-go-pie clean
