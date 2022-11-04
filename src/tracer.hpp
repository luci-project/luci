#pragma once
/*
#include <dlh/container/hash.hpp>

class Tracer {


 public:
	 /// Allow tracing of calling process 
	static bool enable();

	bool loop();
};

bool Tracer::enable() {
	pid_t pid = Syscall::getpid();
	if (auto ptrace_traceme = Syscall::ptrace(PTRACE_TRACEME); ptrace_traceme.failed()) {
		LOG_WARNING << "Unable to enable tracing of PID " << pid << ": " << ptrace_traceme.error_message() << endl;
		return false;
	} else if (auto raise = Syscall::raise(SIGSTOP); raise.failed()) {
		LOG_WARNING << "Unable to raise SIGSTOP in  PID " << pid << " (to wait for tracer): " << raise.error_message() << endl;
		return false;
	} else {
		LOG_INFO << "Tracing of PID " << pid << " enabled" << endl;
		sync.lock();
		tracees.insert(data);
		sync.unlock();
		return true;
	}
}

bool Tracer::loop() {
	while (true) {
		if (auto wait = Syscall::wait(0)) {

		} else if (wait.error() == Errno::ECHILD) {
			LOG_INFO << "Tracer has no childs -- : " << wait.error_message() << endl;

		} else {
			LOG_WARNING << "Tracer waiting for child failed: " << wait.error_message() << endl;
			return false;
		}
	}

}
*/
