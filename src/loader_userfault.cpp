#include "loader.hpp"

#include <dlh/syscall.hpp>
#include <dlh/error.hpp>
#include <dlh/file.hpp>
#include <dlh/log.hpp>

static void userfault_signal(int signum) {
	LOG_INFO << "userfault handler ends (Signal " << signum << ")" << endl;
	Syscall::exit(EXIT_SUCCESS);
}

void Loader::userfault_handler() {
	// Set signal handler
	struct sigaction action;
	Memory::set(&action, 0, sizeof(struct sigaction));
	action.sa_handler = userfault_signal;

	if (auto sigaction = Syscall::sigaction(SIGTERM, &action, NULL); sigaction.failed())
		LOG_WARNING << "Unable to set userfault signal handler: " << sigaction.error_message() << endl;

	if (auto prctl = Syscall::prctl(PR_SET_PDEATHSIG, SIGTERM); prctl.failed())
		LOG_WARNING << "Unable to set userfault death signal: " << prctl.error_message() << endl;

	// Loop over userfaultfd
	static struct uffd_msg msg;
	while (true) {
		auto read = Syscall::read(userfaultfd, &msg, sizeof(msg));

		if (read.failed() && read.error() != EAGAIN) {
			LOG_ERROR << "Reading userfault failed: " << read.error_message() << endl;
			break;
		}

		ssize_t len = read.value();
		if (len == 0) {
			LOG_ERROR << "EOF on userfaultfd" << endl;
			break;
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
						for (MemorySegment &mem: obj->memory_map) {
							if (msg.arg.pagefault.address >= mem.target.page_start() && msg.arg.pagefault.address < mem.target.page_end()) {
								memseg = &mem;
								LOG_DEBUG << "Usefault found memory segment " << reinterpret_cast<void*>(mem.target.address()) << " with " << mem.target.size << " bytes"
								          << " (Source " << mem.source.object << " at " << reinterpret_cast<void*>(mem.source.offset) << " with " << mem.source.size << " bytes)"
								          << " for pagefault at " << reinterpret_cast<void*>(msg.arg.pagefault.address) << endl;

								// Copy data
								if (mem.source.size > 0) {
									cpy.src = mem.source.offset - (mem.target.address() - mem.target.page_start());
									cpy.dst = mem.target.page_start();
									cpy.len = mem.target.page_size();
									// TODO For zeroing set cpy.mode = UFFDIO_COPY_MODE_DONTWAKE; and issue UFFDIO_WAKE at the end
								}
								// zero
								if (mem.source.size < mem.target.size) {
									LOG_ERROR << "Usefault memory segment " << reinterpret_cast<void*>(mem.target.address()) << " with " << mem.target.size << " bytes"
									          << " (Source " << mem.source.object << " at " << reinterpret_cast<void*>(mem.source.offset) << " with " << mem.source.size << " bytes)"
									          << " for pagefault at " << reinterpret_cast<void*>(msg.arg.pagefault.address) << " requires zeroing memory -- but not implemented!" << endl;
									// TODO: For data or rel obj: What about relocations?
								}
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
						LOG_ERROR << "Userfault copy failed: " << ioctl.error_message() << endl;
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
				continue;
		}
	}
	LOG_INFO << "Userfault handler thread ends." << endl;
}
