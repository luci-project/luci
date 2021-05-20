#include "process.hpp"

#include <cstring>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/mman.h>

#include <iterator>

#include "generic.hpp"

Process::Process(uintptr_t stack_pointer, size_t stack_size) : stack_pointer(stack_pointer), stack_size(stack_size) {
	if (this->stack_size == 0) {
		struct rlimit l;
		errno = 0;
		if (getrlimit(RLIMIT_STACK, &l) == -1) {
			LOG_ERROR << "Reading systems default stack limit failed: " << strerror(errno) << endl;
			exit(EXIT_FAILURE);
		} else {
			this->stack_size = reinterpret_cast<size_t>(l.rlim_cur);
			LOG_INFO << "Current system stack size is " << this->stack_size << " bytes" << endl;
		}
	}

	// Allocate stack
	if (this->stack_pointer == 0 && (this->stack_pointer = allocate_stack(this->stack_size)) == 0) {
		LOG_ERROR << "Cannot create process without stack!" << endl;
		exit(EXIT_FAILURE);
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
		aux.insert({ t, v });
	}

	// Adjust
//	aux[AT_RANDOM] = address + (aux[AT_RANDOM] & 0xfff);
}

uintptr_t Process::allocate_stack(size_t stack_size) {
	errno = 0;
	void *stack = ::mmap(NULL, stack_size, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_STACK | MAP_ANONYMOUS, -1, 0);
	if (stack == MAP_FAILED) {
		LOG_ERROR << "Mapping Stack with " << stack_size << " Bytes failed: " << strerror(errno) << endl;
		return 0;
	} else {
		LOG_DEBUG << "Stack at " << stack << " with " << stack_size << endl;
		return reinterpret_cast<uintptr_t>(stack) + stack_size - sizeof(void*);
	}
}


void Process::init(const std::vector<const char *> &arg) {
	if (LOG.visible(Log::INFO)) {
		LOG_INFO << "Starting process with arguments:" << endl;
		for (const char * a : arg)
			LOG << ' ' << a;
		LOG << endl;
	}

	// End marker
	assert(reinterpret_cast<uintptr_t>(stack_pointer) % 8 == 0);
	*reinterpret_cast<void**>(stack_pointer) = NULL;
	stack_pointer -= sizeof(void*);

	// environment strings
	std::vector<const char *> env_str;
	for (auto it = env.rbegin(); it != env.rend(); ++it) {
		stack_pointer -= strlen(*it) + 1;
		env_str.push_back(strcpy(reinterpret_cast<char *>(stack_pointer), *it));
	}

	// argument strings
	std::vector<const char *> arg_str;
	for (auto it = arg.rbegin(); it != arg.rend(); ++it) {
		stack_pointer -= strlen(*it) + 1;
		arg_str.push_back(strcpy(reinterpret_cast<char *>(stack_pointer), *it));
	}

	// padding
	stack_pointer -= stack_pointer % 16;

	Auxiliary * aux_addr = reinterpret_cast<Auxiliary *>(stack_pointer);
	// Auxilary vectors
	aux_addr--;
	aux_addr->a_type = Auxiliary::AT_NULL;
	aux_addr->a_un.a_val = 0;
	for (auto it = aux.begin(); it != aux.end(); ++it) {
		aux_addr--;
		aux_addr->a_type = it->first;
		aux_addr->a_un.a_val = it->second;
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
	LOG_INFO << "Starting process at " << (void*)entry << " (with sp = " << (void*)stack_pointer << ")" << endl;
	asm (
		"mov    %0,%%rsp;"
		"mov    %1,%%rbx;"
		"mov    $0,%%rdx;"
		"jmp    *%%rbx;"
		:: "r" (stack_pointer), "r" (entry)
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
