#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static void* foo(void * param) {
	puts("foo: mutex locking...");
	pthread_mutex_lock(&lock);
	puts("foo: mutex locked");
	sleep(1);
	puts("foo: waiting on condition variable");
	pthread_cond_wait(&cond, &lock);
	puts("foo: mutex unlock");
	pthread_mutex_unlock(&lock);
	return param;
}

static void* bar(void * param) {
	puts("bar: mutex locking...");
	pthread_mutex_lock(&lock);
	puts("bar: mutex locked");
	sleep(1);
	puts("bar: signaling condition variable");
	pthread_cond_signal(&cond);
	sleep(1);
	puts("bar: mutex unlock");
	pthread_mutex_unlock(&lock);
	return param;
}

int main() {
	pthread_t thread_foo, thread_bar;

	pthread_create(&thread_foo, NULL, foo, NULL);
	sleep(2);
	puts("main: mutex locking...");
	pthread_mutex_lock(&lock);
	puts("main: mutex locked");
	pthread_create(&thread_bar, NULL, bar, NULL);
	sleep(1);
	puts("main: mutex unlock");
	pthread_mutex_unlock(&lock);

	pthread_join(thread_foo, NULL);
	pthread_join(thread_bar, NULL);

	return 0;
}
