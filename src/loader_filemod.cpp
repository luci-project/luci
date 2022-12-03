#include "loader.hpp"

#include <dlh/syscall.hpp>
#include <dlh/error.hpp>
#include <dlh/file.hpp>
#include <dlh/log.hpp>

static void filemodification_signal(int signum) {
	LOG_INFO << "File modification handler ends (Signal " << signum << ")" << endl;
	Syscall::exit(EXIT_SUCCESS);
}

void Loader::filemodification_handler() {
	// Set signal handler
	struct sigaction action;
	Memory::set(&action, 0, sizeof(struct sigaction));
	action.sa_handler = filemodification_signal;

	if (auto sigaction = Syscall::sigaction(SIGTERM, &action, NULL); sigaction.failed())
		LOG_WARNING << "Unable to set file modification signal handler: " << sigaction.error_message() << endl;

	if (auto prctl = Syscall::prctl(PR_SET_PDEATHSIG, SIGTERM); prctl.failed())
		LOG_WARNING << "Unable to set file modification death signal: " << prctl.error_message() << endl;

	// Loop over inotify
	char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
	while (true) {
		auto read = Syscall::read(filemodification_inotifyfd, buf, sizeof(buf));

		if (read.failed() && read.error() != EAGAIN) {
			LOG_ERROR << "Reading file modifications inotify failed: " << read.error_message() << endl;
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
				lookup_sync.write_lock();
				bool do_relocate = false;
				for (auto & object_file : lookup)
					if (event->wd == object_file.wd) {
						LOG_DEBUG << "Notification for file modification in " << object_file.path << endl;
						if ((event->mask & IN_IGNORED) != 0) {
							// Reinstall watch
							if (!object_file.watch(true))
								LOG_ERROR << "Unable to watch for updates of " << object_file.path << endl;
						} else if (object_file.load() != nullptr) {
							do_relocate = true;
						}
					}
				if (do_relocate && !relocate(true)) {
					LOG_ERROR << "Updating relocations failed!" << endl;
					assert(false);
				}
				lookup_sync.write_unlock();

				if (config.detect_outdated_access) {
					// Wait a bit to let the update continue
					Syscall::sleep(1);
					LOG_DEBUG << "Disabling old objects" << endl;
					for (auto & object_file : lookup)
						if (object_file.current != nullptr) {
							Object * prev = object_file.current->file_previous;
							if (prev != nullptr)
								prev->disable();
						}
				}
			}
		}
	}
	LOG_INFO << "File modification handler thread ends." << endl;
}
