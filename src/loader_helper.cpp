// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "loader.hpp"

#include <dlh/parser/ar.hpp>
#include <dlh/syscall.hpp>
#include <dlh/error.hpp>
#include <dlh/file.hpp>
#include <dlh/log.hpp>

const unsigned long SECOND_NS = 1'000'000'000UL;

static void helper_signal(int signum) {
	if (signum == SIGTERM) {
		LOG_INFO << "helper loop handler ends (Signal " << signum << ")" << endl;
		Syscall::exit(EXIT_SUCCESS);
	} else {
		LOG_ERROR << "helper loop handler got signal " << signum << " -- exit" << endl;
		Syscall::kill(0, SIGKILL);
		Syscall::exit(128 + signum);
	}
}

void Loader::filemodification_detect(unsigned long now, TreeSet<Pair<unsigned long, ObjectIdentity*>> & worklist_load) {
	char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
	auto read = Syscall::read(filemodification_inotifyfd, buf, sizeof(buf));

	if (read.failed() && read.error() != EAGAIN) {
		LOG_ERROR << "Reading file modifications inotify failed: " << read.error_message() << endl;
		return;
	}

	ssize_t len = read.value();
	if (len <= 0)
		return;
	char *ptr = buf;
	while (ptr < buf + len) {
		const struct inotify_event * event = reinterpret_cast<const struct inotify_event *>(ptr);
		ptr += sizeof(struct inotify_event) + event->len;
		assert((event->mask & IN_ISDIR) == 0);
		bool check_all = false;
		if ((event->mask & IN_Q_OVERFLOW) != 0) {
			LOG_WARNING << "Notification event queue overflow -- will check all objects!" << endl;
			check_all = true;
		}
		if (event->wd != -1) {
			// Get Object
			GuardedWriter _{lookup_sync};
			for (auto & object_file : lookup) {
				if (check_all /* && object_file.flags.updatable == 1 */) {
					worklist_load.emplace(now + SECOND_NS, &object_file);
				} else if (event->wd == object_file.wd) {
					/*if (object_file.flags.updatable == 1) {
						LOG_ERROR << "Unable to update " << object_file << " since it is marked as non updateable!" << endl;
					} else */ if ((event->mask & IN_IGNORED) != 0) {
						// Reinstall watch
						if (!object_file.watch(true))
							LOG_INFO << "Unable to watch for updates of " << object_file.path << endl;
					} else {
						// Ignore update if already in list
						auto it = worklist_load.begin();
						for (; it != worklist_load.end() && it->second != &object_file; ++it) {}
						if (it != worklist_load.end()) {
							if (it->first == now + SECOND_NS) {
								LOG_TRACE << "Skip notification for file modification in " << object_file.path << " since it is already in worklist"<< endl;
							} else {
								LOG_DEBUG << "Skip notification for file modification in " << object_file.path << " since it is already in worklist, but update time" << endl;
								auto && e = move(worklist_load.extract(it));
								e.value().first = now + SECOND_NS;
								worklist_load.insert(move(e));
							}
						} else {
							LOG_DEBUG << "Notification for file modification in " << object_file.path << endl;
							worklist_load.emplace(now + SECOND_NS, &object_file);
						}
					}
				}
			}
		}
	}
}

bool Loader::filemodification_load_helper(ObjectIdentity* object, uintptr_t addr) {  // NOLINT
	auto format = addr != 0 ? File::contents::format(reinterpret_cast<const char *>(addr), 6) : File::contents::format(object->path.c_str());
	switch (format) {
		case File::contents::FORMAT_AR:
		 {
			AR archive(object->path.c_str());
			if (archive.is_valid())
				for (auto & entry : archive)
					if (entry.is_regular() && entry.name() == object->name) {
						auto addr = reinterpret_cast<uintptr_t>(entry.data());
						auto size = entry.size();
						if (auto anon = Syscall::mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0))
							addr = Memory::copy(anon.value(), addr, size);
						return filemodification_load_helper(object, addr);
					}
			break;
		}
		case File::contents::FORMAT_ELF:
			if (object->load(addr) != nullptr)
				return true;
			break;
		default:
			LOG_ERROR << "Invalid file format (" << format_description(format) << ") for updated " << *object << endl;
	}
	return false;
}

void Loader::filemodification_load(unsigned long now, TreeSet<Pair<unsigned long, ObjectIdentity*>> & worklist_load, TreeSet<Pair<unsigned long, Object*>> & worklist_protect) {
	GuardedWriter _{lookup_sync};
	bool updated = false;
	while (!worklist_load.empty()) {
		auto i = worklist_load.lowest();
		if (i->first <= now) {
			assert(i->second != nullptr);
			LOG_INFO << "Loading " << *(i->second) << endl;
			if (filemodification_load_helper(i->second)) {
				updated = true;
				if (config.detect_outdated != Loader::Config::DETECT_OUTDATED_DISABLED) {
					assert(i->second->current != nullptr && i->second->current->file_previous != nullptr);
					worklist_protect.emplace(now + config.detect_outdated_delay * SECOND_NS, i->second->current->file_previous);
				}
			}
			worklist_load.erase(i);
		} else {
			break;
		}
	}
	if (updated && !relocate(true)) {
		LOG_ERROR << "Updating relocations failed!" << endl;
		assert(false);
	}
}

void Loader::filemodification_protect(unsigned long now, TreeSet<Pair<unsigned long, Object*>> & worklist_protect) {
	GuardedWriter _{lookup_sync};
	bool updated = false;
	while (!worklist_protect.empty()) {
		auto i = worklist_protect.lowest();
		if (i->first <= now) {
			assert(i->second != nullptr);
			LOG_INFO << "Protecting " << *(i->second) << endl;
			i->second->disable();
			worklist_protect.erase(i);
		} else {
			break;
		}
	}
}

void Loader::userfault_handle() {
	struct uffd_msg msg;

	auto read = Syscall::read(userfaultfd, &msg, sizeof(msg));

	if (read.failed() && read.error() != EAGAIN) {
		LOG_ERROR << "Reading userfault failed: " << read.error_message() << endl;
		return;
	}

	ssize_t len = read.value();
	if (len == 0) {
		LOG_ERROR << "EOF on userfaultfd" << endl;
		return;
	}

	switch (msg.event) {
		case uffd_msg::UFFD_EVENT_PAGEFAULT:
		{
			LOG_DEBUG << "Pagefault at " << reinterpret_cast<void*>(msg.arg.pagefault.address) << " (flags " << reinterpret_cast<void*>(msg.arg.pagefault.flags) << ")" << endl;

			MemorySegment * memseg = nullptr;
			uffdio_copy cpy;
			/* TODO: Zero
			uffdio_zeropage zero;
			uintptr_t zerosub_start = 0;
			size_t zerosub_len = 0;
			*/

			lookup_sync.read_lock();
			/* Check for every memory segment in every version of each object.
			   This is quite expensive (and could be speed up), however, this should happen only rarely
			*/
			for (auto & object_file : lookup) {
				for (Object * obj = object_file.current; obj != nullptr; obj = obj->file_previous) {
					for (MemorySegment &mem : obj->memory_map) {
						if (msg.arg.pagefault.address >= mem.target.page_start() && msg.arg.pagefault.address < mem.target.page_end()) {
							memseg = &mem;
							LOG_WARNING << "Detected reusing old memory segment " << reinterpret_cast<void*>(mem.target.address()) << " with " << mem.target.size << " bytes"
							            << " (Source " << mem.source.object << " at " << reinterpret_cast<void*>(mem.source.offset) << " with " << mem.source.size << " bytes)"
							            << " due to userspace pagefault at " << reinterpret_cast<void*>(msg.arg.pagefault.address) << endl;

							// Notify
							mem.source.object.file.status(ObjectIdentity::INFO_FAILED_REUSE);

							// Copy data
							if (mem.buffer == 0) {
								LOG_WARNING << "No memory backbuffer for " << reinterpret_cast<void*>(mem.target.page_start()) << " (" <<  mem.target.page_size() << " Bytes) -- copying from source" << endl;
								cpy.src = mem.source.object.data.addr + mem.source.offset - (mem.target.address() - mem.target.page_start());
							} else {
								cpy.src = mem.buffer;
							}
							cpy.dst = mem.target.page_start();
							cpy.len = mem.target.page_size();
							break;
						}
					}
					if (memseg != nullptr)
						break;
				}
				if (memseg != nullptr)
					break;
			}
			lookup_sync.read_unlock();

			while (cpy.src != 0) {
				if (auto ioctl = Syscall::ioctl(userfaultfd, UFFDIO_COPY, &cpy)) {
					memseg->target.status = MemorySegment::MEMSEG_REACTIVATED;
					LOG_DEBUG << "Userfault copied " << cpy.copy << " bytes from " << reinterpret_cast<void*>(cpy.src) << " to " << reinterpret_cast<void*>(cpy.dst) << endl;
					break;
				} else if (ioctl.error() == EAGAIN) {
					LOG_INFO << "Userfault copy incomplete (" << cpy.copy << " / " << cpy.len << " bytes) -- retry"<< endl;
					continue;
				} else {
					LOG_ERROR << "Userfault copy of  " << cpy.len << " bytes from " << reinterpret_cast<void*>(cpy.src) << " to " << reinterpret_cast<void*>(cpy.dst) << " failed: " << ioctl.error_message() << endl;
					break;
				}
			}
			break;
		}
		case uffd_msg::UFFD_EVENT_FORK:
			LOG_DEBUG << "Fork, child's userfaultfd is " << msg.arg.fork.ufd << endl;
			break;
		case uffd_msg::UFFD_EVENT_REMAP:
			LOG_DEBUG << "Remap from " << reinterpret_cast<void*>(msg.arg.remap.from) << " to " << reinterpret_cast<void*>(msg.arg.remap.to) << " (" << msg.arg.remap.len << " bytes)" << endl;
			break;
		case uffd_msg::UFFD_EVENT_REMOVE:
			LOG_DEBUG << "Remove mem area from " << reinterpret_cast<void*>(msg.arg.remove.start) << " to " << reinterpret_cast<void*>(msg.arg.remove.end) << endl;
			break;
		case uffd_msg::UFFD_EVENT_UNMAP:
			LOG_DEBUG << "Unmap mem area from " << reinterpret_cast<void*>(msg.arg.remove.start) << " to " << reinterpret_cast<void*>(msg.arg.remove.end) << endl;
			break;
		default:
			LOG_WARNING << "Invalid userfault event " << msg.event << endl;
	}
}

void Loader::helper_loop() {
	// Set signal handler
	struct sigaction action;
	Memory::set(&action, 0, sizeof(struct sigaction));
	action.sa_handler = helper_signal;

	if (auto sigaction = Syscall::sigaction(SIGTERM, &action, nullptr); sigaction.failed())
		LOG_WARNING << "Unable to set helper loop signal handler for SIGTERM: " << sigaction.error_message() << endl;
	if (auto sigaction = Syscall::sigaction(SIGABRT, &action, nullptr); sigaction.failed())
		LOG_WARNING << "Unable to set helper loop signal handler for SIGABRT: " << sigaction.error_message() << endl;
	if (auto sigaction = Syscall::sigaction(SIGSEGV, &action, nullptr); sigaction.failed())
		LOG_WARNING << "Unable to set helper loop signal handler for SIGSEGV: " << sigaction.error_message() << endl;
	if (auto sigaction = Syscall::sigaction(SIGILL, &action, nullptr); sigaction.failed())
		LOG_WARNING << "Unable to set helper loop signal handler for SIGILL: " << sigaction.error_message() << endl;

	if (auto prctl = Syscall::prctl(PR_SET_PDEATHSIG, SIGTERM); prctl.failed())
		LOG_WARNING << "Unable to set helper loop death signal: " << prctl.error_message() << endl;

	// Loop over inotify
	struct pollfd fds[2];
	fds[0].fd = filemodification_inotifyfd;
	fds[0].events = POLLIN;

	fds[1].fd = userfaultfd;
	fds[1].events = POLLIN;

	unsigned long nfds = userfaultfd == -1 ? 1 : 2;

	TreeSet<Pair<unsigned long, ObjectIdentity*>> worklist_load;
 	TreeSet<Pair<unsigned long, Object*>> worklist_protect;

	while (true) {
		if (auto poll = Syscall::poll(fds, nfds, 1000)) {
			struct timespec time = { 0, 0 };
			auto gettime = Syscall::clock_gettime(CLOCK_MONOTONIC_COARSE, &time);
			auto now = time.nanotimestamp();
			if (gettime.failed()) {
				LOG_ERROR << "Get monotonic coarse clock time failed: " << gettime.error_message() << endl;
			}
			if (poll.value() > 0 && (fds[0].revents & POLLIN) != 0)
				filemodification_detect(now, worklist_load);
			if (poll.value() > 0 && nfds > 1 && (fds[1].revents & POLLIN) != 0)
				userfault_handle();
			if (!worklist_load.empty())
				filemodification_load(now, worklist_load, worklist_protect);
			if (!worklist_protect.empty())
				filemodification_protect(now, worklist_protect);
		} else {
			LOG_ERROR << "Poll of helper loop failed: " << poll.error_message() << endl;
			break;
		}
	}
	LOG_INFO << "File helper loop thread ends." << endl;
}
