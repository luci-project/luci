#include "tracer.hpp"
/*
namespace Tracer {

class Breakpoint {
	const long int3 = 0xcc;

	// TODO: per pid?
	HashMap<void*, unsigned long> data;

	bool read(pid_t pid, void * addr, unsigned long &content) const {
		if (auto ptrace = Syscall::ptrace(PTRACE_PEEKTEXT, pid, addr)) {
			content = ptrace.value();
			return true;
		} else {
			LOG_WARNING << "Unable to read at " << addr << " in PID " << pid << ':' << ptrace.error_message() << endl;
			return false;
		}
	}

	bool write(pid_t pid, void * addr, unsigned long content) const {
		if (auto ptrace = Syscall::ptrace(PTRACE_POKETEXT, pid, addr, content)) {
			return true;
		} else {
			LOG_WARNING << "Unable to write to " << addr << " in PID " << pid << ':' << ptrace.error_message() << endl;
			return false;
		}
	}

 public:
	bool add(pid_t pid, void * addr) {
		unsigned long content;
		if (data.contains(addr)) {
			content = data[addr];
		} else if (!read(pid, addr, content)) {
			return false;
		}

		// Preserve
		data[addr] = content;
		// Change to int3
		content &= ~0xff;
		content |= int3;

		// Write back to addr
		return set(pid, addr, content);
	}

	bool remove(pid_t pid, void * addr) {
		if (!data.contains(addr)) {
			LOG_WARNING << "No knowledge about contents at " << addr << " in PID " << pid << endl;
			return false;
		}

		// Write back to addr
		return set(pid, addr, data[addr]);
	}

	void* handle(pid_t pid) {
		// Read registers
		struct user_regs_struct regs;
		if (auto ptrace_getregs = Syscall::ptrace(PTRACE_GETREGS, pid, nullptr, &regs); ptrace_getregs.failed()) {
			LOG_WARNING << "Unable to get registers of PID " << pid << ':' << ptrace_getregs.error_message() << endl;
			return nullptr;
		}

		// Read address from instruction pointer
		void * addr = reinterpret_cast<void*>(--regs.rip);

		// Check if we have break point information
		if (!data.contains(addr)) {
			LOG_DEBUG << "No known breakpoint at " << addr << " (in PID " << pid << ")" << endl;
			return nullptr;
		}
		unsigned long content = data[addr];

		// (Temporarily) remove breakpoint
		if (!set(pid, addr, content))
			return nullptr;

		// Set register (with updated instruction pointer)
		if (auto ptrace_setregs = Syscall::ptrace(PTRACE_SETREGS, pid, nullptr, &regs); ptrace_setregs.failed()) {
			LOG_ERROR << "Unable to set registers of PID " << pid << ':' << ptrace_setregs.error_message() << endl;
			return nullptr;
		}

		// Step one instruction
		if (auto ptrace_step = Syscall::ptrace(PTRACE_SINGLESTEP, pid); ptrace_step.failed()) {
			LOG_ERROR << "Unable to step single instruction in PID " << pid << ':' << ptrace_step.error_message() << endl;
			return nullptr;
		}

		// Wait for next instruction
		int status = 0;
		if (auto waitpid = Syscall::waitpid(pid, &status)) {
			if (WIFEXITED(status)) {
				LOG_INFO << "PID " << pid << " exited" << endl;
				return addr;
			}
		} else {
			LOG_ERROR << "Waiting for PID " << pid << " failed:" << waitpid.error_message() << endl;
			return nullptr;
		}

		// Add breakpoint
		content &= ~0xff;
		content |= int3;
		if (!set(pid, addr, content))
			return nullptr;

		// Continue execution
		if (auto ptrace_cont = Syscall::ptrace(PTRACE_SINGLESTEP, pid); ptrace_cont.failed()) {
			LOG_ERROR << "Unable to continue execution of PID " << pid << ':' << ptrace_cont.error_message() << endl;
			return nullptr;
		}

		// return handled address
		return addr;
	}
};
static HashMap<pid_t,Breakpoint> breakpoints;
// Map Thread PID to Breakpoint PID
static HashMap<pid_t,pid_t> mapping;

static int pipefd[2];
static bool seized;
static const int tracee_options = PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK;

static bool is_ptrace_event(int status, int event) {
	return (status >> 8) == ((seized ? PTRACE_EVENT_STOP : SIGTRAP) | (event << 8)) || ;
}

bool add()

static bool tracer_loop() {
	while (true) {
		int status = 0;
		pid_t child = wait(&status);

		if (WIFEXITED(status)) {
			LOG_INFO << "Child with PID " << child << " exited with status " << WEXITSTATUS(status) << endl;
			// TODO: Cleanup pid

		} else if (WIFSTOPPED(status)) {

		} else {
			LOG_WARNING << "Child with PID " << child << " is NOT (!) stopped; Status " << (void*)status << endl;
		}

	}
}

bool start(bool seize) {
	seized = seize;
	pid_t parent = Syscall::getpid();

	if (auto fork = Syscall::fork()) {
		if (fork.value() == 0) {
			// child
			pid_t self = Syscall::getpid();
			if (!seize) {
				if (auto ptrace = Syscall::ptrace(PTRACE_TRACEME, parent); ptrace.failed()) {
					LOG_ERROR << "Unable to enable tracing (by PID " << parent << ") in child " << self << ": " << ptrace.error_message() << endl;
					return false;
				}
				if (auto raise = Syscall::raise(SIGSTOP); raise.failed()) {
					LOG_ERROR << "Unable to raise SIGSTOP signal in child " << self << ": " << raise.error_message() << endl;
					return false;
				}
			}
			LOG_INFO << "PID " << self << " will be traced by " << parent << endl;
			return true;
		} else {
			// parent
			assert(fork.value() > 0);
			if (seize) {
				if (auto ptrace = Syscall::ptrace(PTRACE_SEIZE, fork.value(), 0, tracee_options); ptrace.failed()) {
					LOG_ERROR << "Not able to seize tracee with PID " << fork.value() << ": " << ptrace.error_message() endl;
					return false;
				}
			} else {
				if (auto waitpid = Syscall::waitpid(fork.value()); waitpid.failed()) {
					LOG_ERROR << "Waiting for PID " << fork.value() << " failed: " << waitpid.error_message() << endl;
					return false;
				}
				if (auto ptrace = Syscall::ptrace(PTRACE_SETOPTIONS, fork.value(), 0, tracee_options); ptrace.failed()) {
					LOG_WARNING << "Not able to set options on tracee with PID " << ptrace.value() << endl;
				}
				if (auto ptrace = Syscall::ptrace(PTRACE_CONT, fork.value()); ptrace.failed()) {
					LOG_ERROR << "Continuing child with PID " << fork.value() << " failed: " << ptrace.error_message() << endl;
					return false;
				}
			}
			LOG_INFO << "Tracer at PID " << parent << " will trace " << fork.value() << endl;

			return tracer_loop();
		}
	} else {
		LOG_ERROR << "Fork of PID " << parent << " for tracing failed:" << fork.error_message() << endl;
		return false;
	}
}

static void tracer_signal(int signum) {
	LOG_INFO << "Tracer ends (Signal " << signum << ")" << endl;
	Syscall::exit(EXIT_SUCCESS);
}

void Loader::tracer_loop() {
	// Set signal handler
	struct sigaction action;
	Memory::set(&action, 0, sizeof(struct sigaction));
	action.sa_handler = observer_signal;

	if (auto sigaction = Syscall::sigaction(SIGTERM, &action, NULL); sigaction.failed())
		LOG_WARNING << "Unable to set tracer signal handler: " << sigaction.error_message() << endl;

	if (auto prctl = Syscall::prctl(PR_SET_PDEATHSIG, SIGTERM); prctl.failed())
		LOG_WARNING << "Unable to set tracer death signal: " << prctl.error_message() << endl;

	// Loop over inotify
	char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
	while (true) {
		auto read = Syscall::read(inotifyfd, buf, sizeof(buf));

		if (read.failed() && read.error() != EAGAIN) {
			LOG_ERROR << "Reading file modifications failed: " << read.error_message() << endl;
			break;
		}

		ssize_t len = read.value();
		if (len <= 0)
			break;

		char *ptr = buf;
		while (ptr < buf + len) {
			const struct inotify_event * event = reinterpret_cast<const struct inotify_event *>(ptr);
			ptr += sizeof(struct inotify_event) + event->len;

			assert((event->mask & IN_ISDIR) == 0);
			if ((event->mask & IN_Q_OVERFLOW) != 0) {
				LOG_WARNING << "Notification event queue overflow!" << endl;
			}
			if (event->wd != -1) {
				// TODO: 1 second delay is just a very dirty hack (in case of symlink delete - create)
				Syscall::sleep(1);

				// Get Object
				GuardedWriter _{lookup_sync};
				for (auto & object_file : lookup)
					if (event->wd == object_file.wd) {
						LOG_DEBUG << "Notification for file modification in " << object_file.path << endl;
						if ((event->mask & IN_IGNORED) != 0) {
							// Reinstall watch
							if (!object_file.watch(true))
								LOG_ERROR << "Unable to watch for updates of " << object_file.path << endl;
						} else if (object_file.load() != nullptr && !relocate(true)) {
							LOG_ERROR << "Relocation failed during update of " << object_file.path << endl;
							assert(false);
						}
						break;
					}
			}
		}
	}
	LOG_INFO << "Observer background thread ends." << endl;
}


if (Thread::create(&kickoff_tracer, this, true) == nullptr) {
	LOG_ERROR << "Creating tracing background thread failed" << endl;
} else {
	LOG_INFO << "Created tracing background thread" << endl;
}

}  // namespace Tracer
*/
