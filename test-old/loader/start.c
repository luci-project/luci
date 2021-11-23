#include <stddef.h>
#include "sys.h"

extern int main();

char** environ = NULL;

void __start(int argc, char **argv) {
	char **envp = argv + argc;
	environ = envp;
	int r = main(argc, argv, envp);
	sys_exit(r);
}

asm(R"(
.globl _start
.type _start, @function
.align 16
_start:
	# Set base pointer to zero (ABI)
	xor %rbp, %rbp

	# 1st arg: argument count
	pop %rdi

	# 2nd arg: argument array
	mov %rsp, %rsi

	# Align stack pointer
	andq $-16, %rsp

	# call helper function
	call __start

	# Endless loop
1:	jmp 1b
)");
