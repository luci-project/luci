#include <dlh/utils/mutex.hpp>
#include <dlh/unistd.hpp>

#include <linux/futex.h>


Mutex::Mutex() : var(FUTEX_UNLOCKED) {}

bool Mutex::lock(const struct timespec * __restrict__ at) {
	auto state = FUTEX_UNLOCKED;
	if (!__atomic_compare_exchange_n(&var, &state, FUTEX_LOCKED, false, __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
		if (state != FUTEX_LOCKED_WITH_WAITERS)
			state = __atomic_exchange_n(&var, FUTEX_LOCKED_WITH_WAITERS, __ATOMIC_RELEASE);

		while (state != FUTEX_UNLOCKED) {
			if (futex((int*)&var, FUTEX_WAIT, FUTEX_LOCKED_WITH_WAITERS, at, NULL, 0) == ETIMEDOUT)
				return false;

			state = __atomic_exchange_n(&var, FUTEX_LOCKED_WITH_WAITERS, __ATOMIC_RELEASE);
		}
	}
	return true;
}

bool Mutex::trylock() {
	auto state = FUTEX_UNLOCKED;
	return __atomic_compare_exchange_n(&var, &state, FUTEX_LOCKED, false, __ATOMIC_RELEASE, __ATOMIC_RELAXED);
}

void Mutex::unlock() {
	if (__atomic_exchange_n(&var,  FUTEX_UNLOCKED, __ATOMIC_RELEASE)  != FUTEX_LOCKED)
		futex((int*)&var, FUTEX_WAKE, 1, NULL, NULL, 0);
}
