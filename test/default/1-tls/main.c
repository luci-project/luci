#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static __thread int m = 2000;
static __thread int n = 1;

static unsigned tm, tn;

void inc(int *ptr) {
	for (int i = 0; i < 2000; i++) {
		__atomic_fetch_add(&tm, m, __ATOMIC_RELAXED);
		__atomic_fetch_add(&tn, n, __ATOMIC_RELAXED);
		usleep(1000);
		*ptr += n++ + --m;
	}
	fprintf(stderr, "increment of *%p = %d (m = %d, n = %d) finished\n", ptr, *ptr, m, n);
}

void *inc_start(void *void_ptr) {
	inc((int *)void_ptr);
	return NULL;
}

int main() {
	int x = 0, y = -4000000, z = 0;
	pthread_t inc_y_thread, inc_z_thread;

	tm = 0;
	tn = 0;
	m = 42;
	n = 23;

	if(pthread_create(&inc_y_thread, NULL, inc_start, &y)) {
		fprintf(stderr, "Error creating y thread\n");
		return 1;
	} else if(pthread_create(&inc_z_thread, NULL, inc_start, &z)) {
		fprintf(stderr, "Error creating z thread\n");
		return 1;
	}

	inc(&x);

	if(pthread_join(inc_y_thread, NULL)) {
		fprintf(stderr, "Error joining y thread\n");
		return 2;
	} if(pthread_join(inc_z_thread, NULL)) {
		fprintf(stderr, "Error joining z thread\n");
		return 2;
	}

	printf("x: %d, y: %d, z: %d, m: %d, n: %d, tm: %u, tn: %u \n", x, y, z, m, n, tm, tn);
	return 0;
}
