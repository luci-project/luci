#pragma once

#include <list>
#include <pthread.h>


struct Object;

struct ObjectObserver {
	ObjectObserver();
	~ObjectObserver();

	bool add(const Object * obj);
	bool remove(const Object * obj);

	void background_thread_routine();

 private:
	int inotifyfd;
	pthread_t background_thread;

	pthread_mutex_t watch_objects_mutex;
	std::list<std::pair<const Object *, int>> watch_objects;
};
