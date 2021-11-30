#define _GNU_SOURCE
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

struct Data {
	int init, child, parent;
};

int main() {

	int memfd = memfd_create("mmaptest", MFD_CLOEXEC);
	if (memfd == -1) {
		perror("memfd");
		return 1;
	}
	if (ftruncate(memfd, sizeof(struct Data)) == -1) {
		perror("ftruncate");
		return 1;
	}
	struct Data * shared = mmap(NULL, sizeof(struct Data), PROT_READ|PROT_WRITE, MAP_SHARED, memfd, 0);
	if (shared == MAP_FAILED) {
		perror("mmap (shared)");
		return 1;
	}

	shared->init = 1337;

	struct Data * private = mmap(NULL, sizeof(struct Data), PROT_READ|PROT_WRITE, MAP_PRIVATE, memfd, 0);
	if (private == MAP_FAILED) {
		perror("mmap (private)");
		return 1;
	}

	private->init *= -1;

	pid_t pid = fork();
	if (pid == -1) {
		perror("fork");
		return 1;
	}

	if (pid == 0) {
		shared->child = 20;
		private->child = -20;
		for (int i = 0; i < 3; i++) {
			sleep(1);
			shared->child++;
			private->child--;
		}
	} else {
		shared->parent = 40;
		private->parent = -40;
		for (int i = 0; i < 2; i++) {
			sleep(1);
			shared->parent++;
			private->parent--;
		}
	}

	if (pid != 0) {
		int status;
		if (waitpid(pid, &status, 0) == -1) {
			perror("waitpid");
			return 1;
		} else if (!WIFEXITED(status)) {
			puts("child failed");
			return 1;
		} else {
			printf("shared->init = %d\n", shared->init);
			printf("shared->child = %d\n", shared->child);
			printf("shared->parent = %d\n", shared->parent);
			printf("private->init = %d\n", private->init);
			printf("private->child = %d\n", private->child);
			printf("private->parent = %d\n", private->parent);
		}
	}

	if (munmap(private, sizeof(struct Data)) == -1) {
		perror("munmap (private)");
		return 1;
	}
	if (munmap(shared, sizeof(struct Data)) == -1) {
		perror("shmdt (shared)");
		return 1;
	}
	if (close(memfd) != 0) {
		perror("close");
		return 1;
	}

	return 0;
}
