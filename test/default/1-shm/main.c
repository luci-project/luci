#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

struct Data {
	int child, parent;
};

int main() {
	int shmid_pre = shmget(IPC_PRIVATE, sizeof(struct Data), 0777|IPC_CREAT);
	int shmid_post = shmget(IPC_PRIVATE, sizeof(struct Data), 0777|IPC_CREAT);

	struct Data * pre = shmat(shmid_pre, NULL, 0);
	if (pre == (void*)-1) {
		perror("shmat (pre)");
		return 1;
	}

	pid_t pid = fork();
	if (pid == -1) {
		perror("fork");
		return 1;
	}

	struct Data * post = shmat(shmid_post, NULL, 0);
	if (post == (void*)-1) {
		perror("shmat (post)");
		return 1;
	}

	if (pid == 0) {
		pre->child = 20;
		post->child = -20;
		for (int i = 0; i < 3; i++) {
			sleep(1);
			pre->child++;
			post->child--;
		}
	} else {
		pre->parent = 40;
		post->parent = -40;
		for (int i = 0; i < 2; i++) {
			sleep(1);
			pre->parent++;
			post->parent--;
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
			printf("pre->child = %d\n", pre->child);
			printf("pre->parent = %d\n", pre->parent);
			printf("post->child = %d\n", post->child);
			printf("post->parent = %d\n", post->parent);
		}
	}

	if (shmdt(post) == -1) {
		perror("shmdt (post)");
		return 1;
	}
	if (shmdt(pre) == -1) {
		perror("shmdt (pre)");
		return 1;
	}

	if (pid != 0) {
		shmctl(shmid_post, IPC_RMID, 0);
		shmctl(shmid_pre, IPC_RMID, 0);
	}

	return 0;
}
