#include "object_observer.hpp"

#include <errno.h>
#include <cstdlib>
#include <sys/inotify.h>
#include <unistd.h>
#include <string.h>

#include "object.hpp"
#include "generic.hpp"


static void * background_thread_start(void * ptr) {
	reinterpret_cast<ObjectObserver *>(ptr)->background_thread_routine();
	return nullptr;
}

ObjectObserver::ObjectObserver() {
	errno = 0;
	if ((inotifyfd = inotify_init1(IN_CLOEXEC)) == -1)
		LOG_ERROR << "Initializing inotify failed: " << strerror(errno);
	else if (::pthread_mutex_init(&watch_objects_mutex, nullptr) != 0)
		LOG_ERROR << "Creating mutex for background thread failed: " << strerror(errno);
	else if (::pthread_create(&background_thread, nullptr, &background_thread_start, this) != 0)
		LOG_ERROR << "Creating background thread failed: " << strerror(errno);
	else if (::pthread_detach(background_thread) != 0)
		LOG_ERROR << "Detaching background thread failed: " << strerror(errno);
	else
		LOG_INFO << "Created observer using background thread";
}

ObjectObserver::~ObjectObserver() {
	if (close(inotifyfd) != 0)
		LOG_ERROR << "Closing inotify failed: " << strerror(errno);
	else if (::pthread_mutex_destroy(&watch_objects_mutex) != 0)
		LOG_ERROR << "Destroying mutex for background thread failed: " << strerror(errno);
	else
		LOG_INFO << "Destroyed observer using background thread";
}

bool ObjectObserver::add(const Object * obj) {
	assert(obj != nullptr);
	pthread_mutex_lock(&watch_objects_mutex);
	errno = 0;
	int wd = inotify_add_watch(inotifyfd, obj->file.path, IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF);
	if (wd == -1) {
		pthread_mutex_unlock(&watch_objects_mutex);
		LOG_ERROR << "Cannot watch for modification of " << obj->file.path << ": " << strerror(errno);
		return false;
	} else {
		watch_objects.emplace_front(obj, wd);
		pthread_mutex_unlock(&watch_objects_mutex);
		return true;
	}
}

bool ObjectObserver::remove(const Object * obj) {
	assert(obj != nullptr);

	for (auto it = watch_objects.begin(); it != watch_objects.end(); ++it)
		if (it->first == obj) {
			errno = 0;
			int wd = it->second;
			watch_objects.erase(it);
			if (inotify_rm_watch(inotifyfd, wd) == -1) {
				LOG_ERROR << "Removing watch for " << obj->file.path << " failed: " << strerror(errno);
				break;
			} else {
				return true;
			}
		}

	LOG_ERROR << "No watch registered for " << obj->file.path;
	return false;
}

void ObjectObserver::background_thread_routine() {
	char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));

	while (true) {
		errno = 0;
		ssize_t len = read(inotifyfd, buf, sizeof(buf));
		if (len == -1 && errno != EAGAIN) {
			LOG_ERROR << "Reading file modifications failed: " << strerror(errno);
			break;
		} else if (len <= 0) {
			break;
		}

		char *ptr = buf;
		while (ptr < buf + len) {
			const struct inotify_event * event = reinterpret_cast<const struct inotify_event *>(ptr);
			ptr += sizeof(struct inotify_event) + event->len;

			// Get Object
			const Object * obj = nullptr;
			pthread_mutex_lock(&watch_objects_mutex);
			for (auto & wo : watch_objects)
				if (event->wd == wo.second) {
					obj = wo.first;
					break;
				}
			pthread_mutex_unlock(&watch_objects_mutex);

			if (obj == nullptr) {
				LOG_DEBUG << "Possible file modification for " << obj->file.path;
				assert((event->mask & IN_ISDIR) == 0);
				// TODO
			}

		}
	}
	LOG_INFO << "Observer background thread ends.";
}
