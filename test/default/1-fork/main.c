#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int main() {
	pid_t pid = fork();
	if (pid == 0) {
		sleep(1);
		pid_t nested_pid = fork();
		if (nested_pid == 0) {
			sleep(1);
			printf("Hello ");
		} else {
			int nested_status;
			waitpid(nested_pid, &nested_status, 0);
			if (WIFSIGNALED(nested_status)){
				puts("Nested Error");
				return 1;
			} else if (WEXITSTATUS(nested_status) != 0){
				puts("Nested Signal");
				return 1;
			}
			sleep(1);
			printf("World");
		}
	} else {
		int status;
		waitpid(pid, &status, 0);

		if (WIFSIGNALED(status)){
			puts("Error");
			return 1;
		} else if (WEXITSTATUS(status) != 0){
			puts("Signal");
			return 1;
		} else {
			puts("!");
		}
	}
	return 0;
}
