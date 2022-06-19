#include "loader.hpp"

#include <dlh/syscall.hpp>
#include <dlh/error.hpp>
#include <dlh/file.hpp>
#include <dlh/log.hpp>

static void observer_signal(int signum) {
	LOG_INFO << "inotify observer ends (Signal " << signum << ")" << endl;
	Syscall::exit(EXIT_SUCCESS);
}

void Loader::observer_loop() {
	// Set signal handler
	struct sigaction action;
	Memory::set(&action, 0, sizeof(struct sigaction));
	action.sa_handler = observer_signal;

	if (auto sigaction = Syscall::sigaction(SIGTERM, &action, NULL); sigaction.failed())
		LOG_WARNING << "Unable to set observer signal handler: " << sigaction.error_message() << endl;

	if (auto prctl = Syscall::prctl(PR_SET_PDEATHSIG, SIGTERM); prctl.failed())
		LOG_WARNING << "Unable to set observer death signal: " << prctl.error_message() << endl;

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
			}
		}
	}
	LOG_INFO << "Observer background thread ends." << endl;
}
