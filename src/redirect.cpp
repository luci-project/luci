// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "redirect.hpp"

#include <dlh/dir.hpp>
#include <dlh/parser/string.hpp>
#include <dlh/log.hpp>
#include <dlh/syscall.hpp>
#include <dlh/mem.hpp>
#include <dlh/utility.hpp>

#include "loader.hpp"
#include "memory_segment.hpp"

namespace Redirect {

/*! \brief alternative stack for trap signal handler */
static char trap_handler_stack[4096 * 4];
static_assert(count(trap_handler_stack) >= SIGSTKSZ);

/*! \brief Single redirection entry */
struct RedirectionEntry {
	/*! \brief Object containing the address to be redirected */
	Object & from_object;

	/*! \brief Target address for redirection */
	uintptr_t to_address;

	/*! \brief Should the dynamic redirection be replaced by a static (permanent) redirection as soon as possible
	           (= when all threads have reached the trap)? */
	enum Type {
		ONLY_DYNAMIC,  /* Keep always dynamic */
		MAKE_STATIC,   /* make static as soon as possible */
		MADE_STATIC    /* Already made static */
	} type;


	/*! \brief Opcode replaced by trap */
	uint8_t previous_opcode[16] = {};

	/*! \brief Trapped threads */
	TreeSet<pid_t> tids{};
};

/*! \brief Collection of active redirection entries */
static HashMap<uintptr_t, RedirectionEntry> redirection_entries;
/*! \brief Helper to synchronize concurrent collection access */
static RWLock redirection_sync;

/*! \brief Helper to get compositing buffer
 * \param object the object containing the address
 * \param address the address in object which should be modified (redirected)
 * \oaram seg if pointer is set, the memory segment will be stored in it
 * \return pointer to the address in compositing buffer
 */
static uint8_t * mem(Object & object, uintptr_t address, MemorySegment ** seg = nullptr) {
	for (auto m : object.memory_map) {
		uintptr_t start = m.target.address();
		if (address >= start && address < start + m.target.size) {
			uintptr_t buffer = m.compose();
			if (buffer != 0) {
				if (seg != nullptr)
					*seg = &m;
				return reinterpret_cast<uint8_t *>(m.compose() + (address - start));
			}
		}
	}
	return nullptr;
}

/*! \brief Check if a string contains (only) numeric characters
 *  \param str string to be checked
 *  \return `true` if length is non-zero and all characters contain numerics
 */
static bool is_numeric(const char * str) {
	if (str == nullptr || *str == '\0')
		return false;
	while (*str != '\0')
		if (*str < '0' || *str > '9')
			return false;
		else
			str++;
	return true;
}

/*! \brief Check if collection contains IDs of all currently active tasks
 *  \param tids collection of Thread IDs
 *  \return `true` if the Thread IDs of all currently active tasks are listed in the collection
 */
static bool all_tasks(TreeSet<pid_t> & tids) {
	pid_t tid;
	auto handler_thread = Loader::instance()->handler_thread;
	pid_t helper_tid = handler_thread != nullptr ? handler_thread->tid : -1;
	for (auto e : Directory("/proc/self/task"))
		if (is_numeric(e.name()) && Parser::string(tid, e.name()) && tid != helper_tid && !tids.contains(tid))
			return false;
	return true;
}

/*! \brief check if a relative jump is possible
 *  \param from address to be redirected
 *  \param to target address
 *  \return `true` if a 32bit relative jump is possible
 */
static bool check_relative_jump(uintptr_t from, uintptr_t to) {
	return Math::abs(from + 5 - to) < (2UL << 31);
}

/*! \brief Create a static (permanent) redirection
 *  This will create a relative jump if target is within 2 GB (5 byte opcode)
 *  or a 16 byte opcode for an absolute jump.
 *  \param object the object containing the address
 *  \param from the address in object which should be modified (redirected)
 *  \param to the target address to redirect to
 */
static bool static_redirect(Object & object, uintptr_t from, uintptr_t to) {
	MemorySegment * seg = nullptr;
	auto m = mem(object, from, &seg);
	if (m == nullptr)
		return false;

	// Check if we could do a relative jmp
	if (check_relative_jump(from, to)) {
		uint32_t rel = static_cast<uint32_t>(from + 5 - to);
		// jmp $rel
		*(m++) = 0xe9;
		*(m++) = (rel >> 0) & 0xff;
		*(m++) = (rel >> 8) & 0xff;
		*(m++) = (rel >> 16) & 0xff;
		*(m++) = (rel >> 24) & 0xff;
	} else {
		uint64_t abs = static_cast<uint64_t>(to);
		// Hacky 64 bit absolute jmp
		// jmp [rip + 0]
		*(m++) = 0xff;
		*(m++) = 0x25;
		*(m++) = 0x00;
		*(m++) = 0x00;
		*(m++) = 0x00;
		*(m++) = 0x00;
		// at rip: the 64 bit address
		*(m++) = (abs >> 0) & 0xff;
		*(m++) = (abs >> 8) & 0xff;
		*(m++) = (abs >> 16) & 0xff;
		*(m++) = (abs >> 24) & 0xff;
		*(m++) = (abs >> 32) & 0xff;
		*(m++) = (abs >> 40) & 0xff;
		*(m++) = (abs >> 48) & 0xff;
		*(m++) = (abs >> 56) & 0xff;
	}
	// Apply changes
	return seg->finalize();
}


/*! \brief Trap signal handler
 *  \brief signal the signal number
 *  \brief si pointer to signal info structure
 *  \brief context pointer to context of process (including registers)
 */
static void trap_handler(int signal, siginfo *si, ucontext *context) {
	(void) si;
	auto & rip = context->uc_mcontext.gregs[REG_RIP];
	if (signal == SIGTRAP) {
		LOG_TRACE << "Got trap signal " << si->si_signo << ':' << si->si_code << " (" << context->uc_mcontext.gregs[REG_TRAPNO] << ") at " << reinterpret_cast<void*>(rip) << endl;
		redirection_sync.read_lock();
		auto i = redirection_entries.find(rip);
		if (i) {
			auto & entry = i->value;
			LOG_DEBUG << "Redirecting " << reinterpret_cast<void*>(rip) << " (" << entry.from_object << ") to " << reinterpret_cast<void*>(entry.to_address) << endl;
			// Set new RIP to the target address
			rip = entry.to_address;
			// In case this should be replaced by an jmp at some point...
			if (entry.type == RedirectionEntry::MAKE_STATIC) {
				// ... we have to wait until every task has reached this point, to ensure that we don't change code currently executed
				auto tid = Syscall::gettid();
				if (!entry.tids.contains(tid)) {
					// We will change the set, hence needing write lock
					redirection_sync.read_unlock();
					redirection_sync.write_lock();
					entry.tids.insert(tid);
					// Check if all tasks of the thread have hit the trap and try to install the static redirect
					if (all_tasks(entry.tids) && static_redirect(entry.from_object, rip, entry.to_address)) {
						LOG_DEBUG << "Installed a static redirection from " << reinterpret_cast<void*>(rip) << " (" << entry.from_object << ") to " << reinterpret_cast<void*>(entry.to_address) << endl;
						entry.type = RedirectionEntry::MADE_STATIC;
					}
					redirection_sync.write_unlock();
					return;
				}
			}
		} else {
			LOG_ERROR << "Trap at " << reinterpret_cast<void*>(rip) << ", but not registered to any code" << endl;
		}
		redirection_sync.read_unlock();
	} else {
		LOG_WARNING << "Got unexpected signal " << si->si_signo << ':' << si->si_code << " at " << reinterpret_cast<void*>(rip) << endl;
	}
}

/*! \brief Helper function to install the trap signal handler
 * can be called multiple times
 * \return `true` if signal handler was installed (at least at some point in the past)
 */
static bool install_trap_handler() {
	static bool installed = false;
	if (!installed) {
		sigstack stack;
		stack.ss_sp = reinterpret_cast<void*>(trap_handler_stack);
		stack.ss_size = count(trap_handler_stack);
		stack.ss_flags = 0;
		if (auto sigaltstack = Syscall::sigaltstack(&stack, NULL)) {
			LOG_DEBUG << "Trap signal handler will use stack at " << stack.ss_sp << " (" << stack.ss_size << " Bytes)" << endl;
		} else {
			LOG_ERROR << "Setting alternative stack " << stack.ss_sp << " (" << stack.ss_size << " Bytes) for trap signal handler failed: " << sigaltstack.error_message() << endl;
			return false;
		}

		sigaction action;
		action.sa_flags = SA_ONSTACK | SA_SIGINFO;
		action.sa_sigaction = trap_handler;      /* Address of a signal handler */
		// TODO: Benchmark performance between trap and illegal opcode signal
		if (auto sigaction = Syscall::sigaction(SIGTRAP, &action, NULL)) {
			installed = true;
			LOG_INFO << "Installed trap signal handler" << endl;
		} else {
			LOG_ERROR << "Unable to install trap signal handler: " << sigaction.error_message() << endl;
			return false;
		}
	}
	return true;
}

bool add(Object & from_object, uintptr_t from_address, uintptr_t to_address, size_t from_size, bool make_static, bool finalize) {
	RedirectionEntry::Type type = RedirectionEntry::ONLY_DYNAMIC;
	size_t bytes_to_be_replaced = 1;
	if (make_static) {
		if (from_size < bytes_to_be_replaced) {
			LOG_INFO << "Not enough space at " << reinterpret_cast<void*>(from_address) << " for static redirection -- will only use dynamic" << endl;
		} else {
			type = RedirectionEntry::MAKE_STATIC;
			bytes_to_be_replaced = check_relative_jump(from_address, to_address) ? 5 : 16;
		}
	}

	// Entry
	GuardedWriter _(redirection_sync);
	auto e = redirection_entries.find(from_address);
	if (e) {
		auto & val = e->value;
		if (val.type == RedirectionEntry::MADE_STATIC) {
			// In case it is not the correct target we can directly change the jump
			if (type == RedirectionEntry::MAKE_STATIC)
				return val.to_address == to_address ? true : static_redirect(from_object, from_address, to_address);
		} else if (&(val.from_object) == &from_object && val.to_address == to_address && val.type == type) {
			// already configured
			return true;
		} else {
			// delete entry
			redirection_entries.erase(from_address);
		}
	}

	MemorySegment *seg = nullptr;
	uint8_t * ptr = mem(from_object, from_address, &seg);
	RedirectionEntry entry{from_object, to_address, type};

	// store previous opcode (in case this should be made static at some point)
	for (size_t i = 0; i < bytes_to_be_replaced; i++)
		entry.previous_opcode[i] = ptr[i];

	// Add entry
	redirection_entries.insert(from_address, entry);

	// Add trap
	*ptr = 0xcc;  // or 0xf1

	// Apply changes
	if (finalize)
		seg->finalize();

	// Install trap signal handler
	return install_trap_handler();
}

bool add(uintptr_t from, uintptr_t to, size_t from_size, bool make_static, bool finalize) {
	Object * object = Loader::instance()->resolve_object(from);
	return object == nullptr ? false : add(*object, from, to, from_size, make_static, finalize);
}

bool remove(uintptr_t address, bool finalize) {
	GuardedWriter _(redirection_sync);
	auto e = redirection_entries.find(address);
	if (e) {
		auto & val = e->value;
		// Recover old opcode
		MemorySegment *seg = nullptr;
		uint8_t * ptr = mem(val.from_object, address, &seg);
		size_t bytes_to_be_recovered = val.type == RedirectionEntry::MADE_STATIC ? (check_relative_jump(address, val.to_address) ? 5 : 16) : 1;
		for (size_t i = 0; i < bytes_to_be_recovered; i++)
			ptr[i] = val.previous_opcode[i];

		// delete entry
		redirection_entries.erase(e);

		// Apply changes
		if (finalize)
			seg->finalize();

		return true;
	} else {
		return false;
	}
}

}  // namespace Redirect
