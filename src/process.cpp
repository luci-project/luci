#include "process.hpp"

#include <dlh/syscall.hpp>
#include <dlh/assert.hpp>
#include <dlh/string.hpp>
#include <dlh/macro.hpp>
#include <dlh/log.hpp>


Process::Process(uintptr_t stack_pointer, size_t stack_size) : stack_pointer(stack_pointer), stack_size(stack_size) {
	if (this->stack_size == 0) {
		struct rlimit l;
		Syscall::getrlimit(RLIMIT_STACK, &l).exit_on_error("Reading systems default stack limit failed");
		this->stack_size = reinterpret_cast<size_t>(l.rlim_cur);
		LOG_INFO << "Current system stack size is " << this->stack_size << " bytes" << endl;
	}

	// Allocate stack
	if (this->stack_pointer == 0 && (this->stack_pointer = allocate_stack(this->stack_size)) == 0) {
		LOG_ERROR << "Cannot create process without stack!" << endl;
		Syscall::exit(EXIT_FAILURE);
	}

	// Read current environment variables
	int envc;
	for (envc = 0 ; environ[envc] != nullptr; envc++)
		if (*environ[envc] != '\0')
			env.push_back(environ[envc]);

	// Read current auxiliary vectors
	Auxiliary * auxv = reinterpret_cast<Auxiliary *>(environ + envc + 1);
	for (int auxc = 0 ; auxv[auxc].a_type != Auxiliary::AT_NULL; auxc++) {
		Auxiliary::type t = auxv[auxc].a_type;
		auto v = auxv[auxc].a_un.a_val;
		aux.insert(t, v);
	}

	// Adjust
//	aux[AT_RANDOM] = address + (aux[AT_RANDOM] & 0xfff);
}

uintptr_t Process::allocate_stack(size_t stack_size) {
	auto mmap = Syscall::mmap(NULL, stack_size, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_STACK | MAP_ANONYMOUS, -1, 0);
	if (mmap.success()) {
		return mmap.value() + stack_size - sizeof(void*);
	} else {
		LOG_ERROR << "Mapping Stack with " << stack_size << " Bytes failed: " << mmap.error_message() << endl;
		return 0;
	}
}


void Process::init(const Vector<const char *> &arg) {
	if (LOG.visible(Log::INFO)) {
		LOG_INFO << "Initialize process with arguments:" << endl;
		for (const char * a : arg)
			LOG << ' ' << a;
		LOG << endl;
	}


	// End marker
	assert(reinterpret_cast<uintptr_t>(stack_pointer) % 8 == 0);
	*reinterpret_cast<void**>(stack_pointer) = NULL;
	stack_pointer -= sizeof(void*);

	// environment strings
	Vector<const char *> env_str;
	for (auto it = env.rbegin(); it != env.rend(); ++it) {
		stack_pointer -= String::len(*it) + 1;
		env_str.push_back(String::copy(reinterpret_cast<char *>(stack_pointer), *it));
	}

	// argument strings
	Vector<const char *> arg_str;
	for (auto it = arg.rbegin(); it != arg.rend(); ++it) {
		stack_pointer -= String::len(*it) + 1;
		arg_str.push_back(String::copy(reinterpret_cast<char *>(stack_pointer), *it));
	}

	// padding
	stack_pointer -= stack_pointer % 16;

	Auxiliary * aux_addr = reinterpret_cast<Auxiliary *>(stack_pointer);
	// Auxilary vectors
	aux_addr--;
	aux_addr->a_type = Auxiliary::AT_NULL;
	aux_addr->a_un.a_val = 0;
	for (const auto & i : aux) {
		aux_addr--;
		aux_addr->a_type = i.key;
		aux_addr->a_un.a_val = i.value;
	}

	const char ** ptr_addr = reinterpret_cast<const char**>(aux_addr);
	// environment pointer
	*(--ptr_addr) = NULL;
	for (auto & e: env_str)
		*(--ptr_addr) = e;
	envp = ptr_addr;

	// argument array
	*(--ptr_addr) = NULL;
	for (auto & e: arg_str) {
		*(--ptr_addr) = e;
	}
	argv = ptr_addr;

	// Set argument count
	argc = arg.size();
	stack_pointer = reinterpret_cast<uintptr_t>(ptr_addr) - sizeof(void*);
	*reinterpret_cast<void**>(stack_pointer) = reinterpret_cast<void*>(argc);
}

void Process::start(uintptr_t entry) {
	start(entry, stack_pointer, envp);
}

static void exit_func() {
	LOG_INFO << "Exit Function called" << endl;
}

void Process::start(uintptr_t entry, uintptr_t stack_pointer, const char ** envp) {
	LOG_INFO << "Starting process at " << (void*)entry << " (with sp = " << (void*)stack_pointer << ")" << endl;
	const unsigned long flags = 1 << 0   // CF: No carry
	                          | 1 << 2   // PF: Even parity
	                          | 1 << 4   // AF: No auxiliary carry
	                          | 1 << 6   // ZF: No zero result
	                          | 1 << 7   // SF: Unsigned result
	                          | 1 << 10  // DF: Direction forward
	                          | 1 << 11  // OF: No overflow occurred
	                          ;
	asm (
		/* Required by Sys V ABI */
		"pushf;"                // Clear several flag set
		"andq   $0,(%%rsp);"
		"popf;"
		"mov    %1,%%rsp;"      // stack pointer
		"mov    %2,%%r12;"      // entry function in r12
		"mov    %3,%%rdx;"      // exit function in rdx
		/* GLIBC sysdeps/x86_64/dl-machine.h */
		"mov    %4,%%rcx;"      // Evnironment pointer in rcx
		"mov    %%rsp,%%r13;"   // stack pointer copy in r13
		/* Sanity */
		"mov    $0x1c,%%rax;"   // rax with (default?) value
		"mov    $0,%%rbx;"      // rbx emptied
		"mov    $0,%%rbp;"      // rbp emptied
		"mov    $0,%%rsi;"      // rsi emptied
		"mov    $0,%%rdi;"      // rdi emptied
		"mov    $0,%%r8;"       // r8 emptied
		"mov    $0,%%r9;"       // r8 emptied
		"mov    $0,%%r10;"      // r10 emptied
		"mov    $0,%%r11;"      // r11 emptied
		"mov    $0,%%r14;"      // r14 emptied
		"mov    $0,%%r15;"      // r54 emptied
		/* Jump to function */
		"jmp    *%%r12;"
		:: "i"(~flags), "r" (stack_pointer), "r" (entry), "r" (exit_func), "r" (envp)
	);
}


void Process::dump(int argc, const char **argv, const char ** envp) {
	long * argcp  = reinterpret_cast<long*>(argv) - 1;
	cout << argcp << ": argc = " << *argcp << endl;

	for (int i = 0 ; i < argc; i++)
		cout << argv + i << ": argv[" << i << "] = " << (void*)argv[i] << " (" << argv[i] << ")" << endl;
	cout << argv + argc << ": argv[" << argc << "] = " << (void*)argv[argc] << endl;

	int envc;
	for (envc = 0 ; envp[envc] != NULL; envc++)
		cout << envp + envc << ": envp[" << envc << "] = " << (void*)envp[envc] << " (" << envp[envc] << ")" << endl;
	cout << envp + envc << ": envp[" << envc << "] = " << (void*)envp[envc] << endl;

	Auxiliary * auxv = reinterpret_cast<Auxiliary *>(envp + envc + 1);
	int auxc;
	for (auxc = 0 ; auxv[auxc].a_type != Auxiliary::AT_NULL; auxc++)
		cout <<  auxv + auxc << ": auxv[" << auxc << "] = " <<  auxv[auxc].a_type << ": " << auxv[auxc].a_un.a_val << endl;
	cout <<  auxv + auxc << ": auxv[" << auxc << "] = " <<  auxv[auxc].a_type << ": " << auxv[auxc].a_un.a_val << endl;
}
