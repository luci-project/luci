#pragma once

#include <sys/time.h>

class Mutex {
	enum State : int {
		FUTEX_UNLOCKED,
		FUTEX_LOCKED,
		FUTEX_LOCKED_WITH_WAITERS
	} var;

 public:
	Mutex();

	bool lock(const struct timespec * __restrict__ at = nullptr);

	bool trylock();

	void unlock();

};
