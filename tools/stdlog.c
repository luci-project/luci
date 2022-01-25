/* STDLOG
enables logging of standard streams

Compile with

	gcc -o stdlog stdlog.c -lutil

Example usage

	./stdlog -o stdout.log -o- -t ls | cat -

or

	/bin/bash -c 'for i in 1 2 3 ; do echo $i ; sleep 1; done' | \
	./stdlog -a -i stdin.log  -i all.log '+%H:%M:%S In:  ' \
	            -o stdout.log -o all.log '+%H:%M:%S Out: ' \
	            -e stderr.log -e all.log '+%H:%M:%S Err: ' \
	/bin/bash -c 'while IFS= read -r s; do echo Got $s! ; echo Boring $s... >&2; done; exit 23'

*/


#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/syscall.h>
#include <linux/limits.h>
#include <pty.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/sendfile.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <signal.h>


/*** pidfd_* syscalls ***/
#ifndef __NR_pidfd_open
#define __NR_pidfd_open 434
#endif
#ifndef __NR_pidfd_send_signal
#define __NR_pidfd_send_signal 424
#endif

static int pidfd_open(pid_t pid, unsigned int flags) {
	return syscall(__NR_pidfd_open, pid, flags);
}

static int pidfd_send_signal(int pidfd, int sig, siginfo_t *info, unsigned int flags) {
   return syscall(__NR_pidfd_send_signal, pidfd, sig, info, flags);
}


/*** output macros ***/
#define error_message(...)                           \
	do {                                             \
		dprintf(STDERR_FILENO, __VA_ARGS__);         \
		write_all(STDERR_FILENO, " - abort!\n", 9);  \
		exit(EXIT_FAILURE);                          \
	} while(0)

#define errno_message(...)                                  \
	do {                                                    \
		dprintf(STDERR_FILENO, __VA_ARGS__);                \
		dprintf(STDERR_FILENO, ": %s\n", strerror(errno));  \
		exit(EXIT_FAILURE);                                 \
	} while(0)

#define verbose_message(...)                      \
	do {                                          \
		if (verbose) {                            \
			dprintf(STDERR_FILENO, __VA_ARGS__);  \
			write_all(STDERR_FILENO, "\n", 1);    \
		}                                         \
	} while(0)



/*** constants ***/
const size_t prefix_size = 80;
const size_t chunk_size = 4096;

const int redirect_signals[] = { SIGCONT, SIGHUP, SIGINT, SIGIO, SIGTTIN, SIGCHLD, SIGTTOU, SIGPIPE, SIGQUIT, SIGTERM, SIGALRM, SIGUSR1, SIGUSR2 };

enum Stream {
	STREAM_IN, STREAM_OUT, STREAM_ERROR, STREAMS
};


/*** helper structures ***/
typedef struct {
	const char * file;
	int fd;
	struct stat stat;
	bool append;
	bool newline;
	bool prefix_relative;
	const char * prefix_format;
	time_t prefix_update;
	size_t prefix_len;
	char prefix[80];
} target_t;


/*** variables **/
static char * stream_name[STREAMS] = { "input", "output", "error" };
static target_t * stream[STREAMS];
static size_t stream_len[STREAMS];
static struct stat stream_stat[STREAMS];
static bool verbose = false;
static bool passthrough_stdout = true;
static bool passthrough_stderr = true;
static int childfd = -1;
static pid_t pid = -1;
static time_t start = 0;
static bool istty[STREAMS];


/*** helper functions **/
static void help(char * name) {
	puts("STDLOG: Log standard input/output/error of command to file(s)");
	printf("\nUsage:\n\t%s (-v|-h)? (-[ioeIOE] FILE (+FORMAT)?)* COMMAND [PARAMS]\n\n", name);
	puts("Parameters:");
	puts(" -i FILE   log standard input to file (truncate if exist)");
	puts(" -I FILE   log standard input to file (append if exist)");
	puts(" -o FILE   log standard output to file (truncate if exist)");
	puts(" -O FILE   log standard output to file (append if exist)");
	puts(" -e FILE   log standard error to file (truncate if exist)");
	puts(" -E FILE   log standard error to file (append if exist)");
	puts(" +FORMAT   prefix every log line with the given strptime(2) compliant string");
	puts("           (must directly preceed one of the above log commands)");
	puts(" @FORMAT   similar to normal format string, however, uses relative timing");
	puts("           (hence only the '%s' format character should be used)");
	puts(" -q        no pass-through of output and error streams");
	puts(" -t        use pseude terminals for all streams");
	puts(" -a        tee all streams to standard output (disables pass-through)");
	puts(" -v        verbose output (on standard error)");
	puts(" -h        show this help");
	puts("");
	puts("Instead of a file name, '-' can be used to redirect to programs standard output");
	puts("and '#' for the standard error stream (truncate/append settings are ignored)");
	puts("Please note: When using such a redirection, the pass-through of this stream will be disabled");
	puts("");
	puts("It is possible to log multiple streams into the same file,");
	puts("however, the mode (append/truncate) must be consistent!");
}


static void write_all(int fd, const void *buf, size_t count) {
	if (count > 0) {
		ssize_t bytes;
		while ((bytes = write(fd, buf, count)) > 0) {
			if (bytes == count)
				return;
			assert(bytes <= count);
			count -= bytes;
		}
		if (bytes == -1)
			errno_message("write %zu bytes to fd %d failed", count, fd);
	}
}


static void redirect(int from, target_t * to, size_t tos, bool ignore_read_error) {
	time_t ts = 0;
	struct tm *tm = NULL;
	struct tm *tr = NULL;

	char buf[chunk_size];
	ssize_t len;

	while ((len = read(from, buf, chunk_size)) > 0) {
		for (size_t t = 0; t < tos; t++) {
			// A prefix requires a byte wise inspection of the output string
			if (to[t].prefix_format != NULL) {
				for (size_t p, s = 0; s < len; s = p) {
					// Print prefix after each newline
					if (to[t].newline) {
						// Is [cached] prefix fresh?
						if (ts != to[t].prefix_update && (ts != 0 || (ts = time(NULL)) != to[t].prefix_update)) {
							if (!to[t].prefix_relative) {
								if (tm == NULL && (tm = localtime(&ts)) == NULL)
									errno_message("Converting %ld to localtime failed", ts);
							} else if (tr == NULL) {
								time_t r = ts - start;
								if ((tr = localtime(&r)) == NULL)
									errno_message("Converting relative %ld to localtime failed", r);
							}
							// Format time to prefix
							if (ts != to[t].prefix_update) {
								to[t].prefix_update = ts;
								to[t].prefix_len = strftime(to[t].prefix, sizeof(to[t].prefix), to[t].prefix_format, to[t].prefix_relative ? tr : tm);
							}
						}
						// Print prefix
						write_all(to[t].fd, to[t].prefix, to[t].prefix_len);
						to[t].newline = false;
					}

					// Find next new line
					for (p = s; p < len && buf[p] != '\n'; p++) {}

					// Check if stopped due to new line
					if (p < len && buf[p] == '\n') {
						to[t].newline = true;
						p++;
					}

					// write
					write_all(to[t].fd, buf + s, p - s);
				}
			} else {
				// Full dump
				write_all(to[t].fd, buf, len);
			}
		}
		if (!ignore_read_error && len == -1 && errno != EWOULDBLOCK)
			errno_message("read failed");
	}
}


static bool epoll_handle(int from, target_t * to, size_t tos, uint32_t event, bool ignore_read_error) {
	switch (event) {
		case EPOLLIN:
			redirect(from, to, tos, ignore_read_error);
			return true;

		case EPOLLERR:
			verbose_message("Observed fd %d failed", from);
			close(from);
			return false;

		case EPOLLHUP:
			verbose_message("Observed fd %d hung up", from);
			close(from);
			return false;
	}

}

static target_t * add_target(enum Stream s) {
	size_t idx = stream_len[s]++;
	if ((stream[s] = realloc(stream[s], sizeof(target_t) * stream_len[s])) == NULL)
		error_message("Unable to allocate %zu bytes of memory!", sizeof(target_t) * stream_len[s]);

	return stream[s] + idx;
}


static void init_target(target_t * target, char * file, bool append) {
	target->newline = true;
	target->prefix_relative = false;
	target->prefix_format = NULL;
	target->prefix_update = 1;
	target->prefix_len = 0;
	if ((file[0] == '-' || file[0] == '#') && file[1] == '\0') {
		// Special case: STDOUT
		target->append = true;
		if (file[0] == '-') {
			target->file = "[STDOUT]";
			target->fd = STDOUT_FILENO;
		} else {
			target->file = "[STDERR]";
			target->fd = STDERR_FILENO;
		}

		passthrough_stdout = false;
		passthrough_stderr = false;

		if (fstat(target->fd, &target->stat) != 0)
			errno_message("fstat");
	} else {
		target->append = append;
		target->file = file;
		// Open
		if ((target->fd = open(file, O_WRONLY | O_CREAT | O_CLOEXEC | O_NONBLOCK | O_NOCTTY, S_IRUSR | S_IWUSR | S_IRGRP)) == -1)
			errno_message("Opening '%s' failed", file);

		if (fstat(target->fd, &target->stat) != 0)
			errno_message("Stat on '%s' failed", file);

		// check if already opened
		for (int s = 0; s < STREAMS; s++)
			for (size_t i = 0; i < stream_len[s]; i++)
				if (&stream[s][i] != target && stream[s][i].stat.st_dev == target->stat.st_dev && stream[s][i].stat.st_ino == target->stat.st_ino) {
					if (append != stream[s][i].append)
						error_message("File %s shall be opened with modes append and truncate!", file);
					close(target->fd);
					target->fd = stream[s][i].fd;
					return;
				}

		// append or truncate
		if (append) {
			if (fcntl(target->fd, F_SETFL, O_APPEND) == -1)
				errno_message("Appending to file %s to failed", file);
		} else if ((target->stat.st_mode & S_IFMT) == S_IFREG) {
			verbose_message("Truncating %s", file);
			if (ftruncate(target->fd, 0) == -1)
				errno_message("Truncating file %s failed", file);
			else if (lseek(target->fd, 0, SEEK_SET) == -1)
				errno_message("Setting offset of file %s to zero failed", file);
		}
	}
}


static void handler(int signo, siginfo_t *info, void *context) {
	verbose_message("Got signal %d.", signo);
	if (childfd != -1 && pidfd_send_signal(childfd, signo, info, 0) == -1)
		kill(pid, signo);
}


int main(int argc, char * argv[]) {
	// Parse arguments
	bool force_tty = false;
	bool show_all = false;
	target_t * current = NULL;
	int a;
	for (a = 1; a < argc; a++) {
		if (argv[a][0] == '-') {
			enum Stream s;
			bool append = false;
			switch (argv[a][1]) {
				case 'I':
					append = true;
				case 'i':
					s = STREAM_IN;
					break;

				case 'O':
					append = true;
				case 'o':
					s = STREAM_OUT;
					break;

				case 'E':
					append = true;
				case 'e':
					s = STREAM_ERROR;
					break;

				case 'h':
					help(argv[0]);
					return EXIT_SUCCESS;

				case 't':
					if (argv[a][2])
						error_message("Invalid option '%s' (did you simply mean '-%c'?)", argv[a], argv[a][1]);
					force_tty = true;
					continue;

				case 'a':
					show_all = true;
				case 'q':
					if (argv[a][2])
						error_message("Invalid option '%s' (did you simply mean '-%c'?)", argv[a], argv[a][1]);
					passthrough_stdout = false;
					passthrough_stderr = false;
					continue;

				case 'v':
					if (argv[a][2])
						error_message("Invalid option '%s' (did you simply mean '-%c'?)", argv[a], argv[a][1]);
					verbose = true;
					continue;

				default:
					error_message("Invalid option '-%c'!", argv[a][1]);
			}

			current = add_target(s);

			char * file = argv[a] + 2;
			if (file[0] == '\0') {
				a++;
				if (a < argc)
					file = argv[a];
				else
					error_message("Missing file name for redirection of standard %s!", stream_name[s]);
			}

			init_target(current, file, append);
			verbose_message("Redirecting %s to '%s'.",  stream_name[s], current->file);
		} else if (argv[a][0] == '+' || argv[a][0] == '@') {
			if (current == NULL)
				error_message("Format '%s' without association to redirection!", argv[a]);
			current->prefix_relative = argv[a][0] == '@';
			current->prefix_format = argv[a] + 1;
			verbose_message("Set format of redirection in '%s' to '%s'.", current->file, current->prefix_format);
			current = NULL;
		} else {
			break;
		}
	}


	if (passthrough_stdout)
		init_target(add_target(STREAM_OUT), "-", false);

	if (passthrough_stderr)
		init_target(add_target(STREAM_ERROR), "#", false);

	if (stream_len[STREAM_IN] == 0 && stream_len[STREAM_OUT] == 0 && stream_len[STREAM_ERROR] == 0)
		verbose_message("No redirection used... why are you using this utility?");


	istty[STREAM_IN] = isatty(STDIN_FILENO);
	istty[STREAM_OUT] = isatty(STDOUT_FILENO);
	istty[STREAM_ERROR] = isatty(STDOUT_FILENO);

	// Default Output
	if (show_all) {
		for (int s = 0; s < STREAMS; s++) {
			target_t * t = add_target(s);
			init_target(t, "-", false);
			t->prefix_relative = istty[STREAM_OUT];
			switch (s) {
				case STREAM_IN:
					t->prefix_format = istty[STREAM_OUT] ? "\e[37m[%5s]\e[0m " : "[%H:%M:%S] STDIN:  ";
					break;
				case STREAM_OUT:
					t->prefix_format = istty[STREAM_OUT] ? "\e[2m[%5s]\e[0m " : "[%H:%M:%S] STDOUT: ";
					break;
				case STREAM_ERROR:
					t->prefix_format = istty[STREAM_OUT] ? "\e[31m[%5s]\e[0m " : "[%H:%M:%S] STDERR: ";
					break;
			}
		}

	}

	// Read terminal settings
	struct termios tio = {};
	struct winsize ws = {};

	if (istty[STREAM_OUT]) {
		if (tcgetattr(STDOUT_FILENO, &tio) != 0)
			verbose_message("Getting current terminal attributes failed: %s", strerror(errno));

		if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0)
			verbose_message("Getting current terminal size failed: %s", strerror(errno));

		verbose_message("The current terminals size is %d x %d", ws.ws_row, ws.ws_col);
	} else {
		verbose_message("Not running in a terminal");
	}

	// Setup PTY
	int master[STREAMS], slave[STREAMS];
	for (int s = 0; s < STREAMS; s++)
		if (stream_len[s] > 0) {
			if (force_tty || istty[s]) {
				if (openpty(master + s, slave + s, NULL, &tio, &ws) != 0)
					errno_message("Opening PTY failed");

				istty[s] = true;
			} else {
				int fd[2];
				if (pipe(fd) != 0)
					errno_message("Creating pipe failed");

				master[s] = s == STREAM_IN ? fd[1] : fd[0];
				slave[s] = s != STREAM_IN ? fd[1] : fd[0];

				istty[s] = false;
			}

			if (fcntl(master[s], F_SETFL, O_NONBLOCK) != 0)
				errno_message("Configuring non-blocking PTY failed");
		} else {
			master[s] = slave[s] = -1;
		}


	// fork
	verbose_message("Preparing fork & exec of '%s'.", argv[a]);

	pid = fork();
	if (pid == -1) {
		errno_message("Forking failed");
	} else if (pid == 0) {
		// child: change standard streams and execute payload
		for (int s = 0; s < STREAMS; s++)
			close(master[s]);

		setsid();

		if (slave[STREAM_OUT] != -1) {
			ioctl(slave[STREAM_OUT], TIOCSCTTY, NULL);
			dup2(slave[STREAM_OUT], STDOUT_FILENO);
			if (fcntl(slave[STREAM_OUT], F_SETFD, FD_CLOEXEC) == -1)
				perror("fcntl/cloexec");
		}

		if (slave[STREAM_IN] != -1) {
			dup2(slave[STREAM_IN], STDIN_FILENO);
			if (fcntl(slave[STREAM_IN], F_SETFD, FD_CLOEXEC) == -1)
				perror("fcntl/cloexec");
		}

		if (slave[STREAM_ERROR] != -1) {
			dup2(slave[STREAM_ERROR], STDERR_FILENO);
			if (fcntl(slave[STREAM_ERROR], F_SETFD, FD_CLOEXEC) == -1)
				perror("fcntl/cloexec");
		}

		if (execvp(argv[a], argv + a) == -1)
			errno_message("Executing '%s' failed", argv[a]);
	} else {
		// parent: wait for (stream/process) events
		start = time(NULL);

		verbose_message("Child at PID %d", pid);
		childfd = pidfd_open(pid, 0);
		if (childfd == -1)
			errno_message("Retrieving file descriptor for child process with PID %d failed", pid);

		struct sigaction sig = { 0 };
		sig.sa_flags = SA_SIGINFO;
		sigemptyset(&sig.sa_mask);
		sig.sa_flags = 0;
		sig.sa_sigaction = &handler;
		for (size_t s = 0; s < sizeof(redirect_signals)/sizeof(redirect_signals[0]); s++)
			if (sigaction(redirect_signals[s], &sig, NULL) == -1)
				errno_message("Installing handler for signal %d failed", redirect_signals[s]);

		int epollfd = epoll_create1(EPOLL_CLOEXEC);
		if (epollfd == -1)
			errno_message("Creating epoll failed");

		struct epoll_event child_ev;
		child_ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
		child_ev.data.fd = childfd;
		if (epoll_ctl(epollfd, EPOLL_CTL_ADD, childfd, &child_ev) == -1)
			errno_message("Adding child process file descriptor to epoll failed");

		if (master[STREAM_IN] != -1) {
			// Setting nonblocking input
			if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) != 0)
				errno_message("Configuring non-blocking standard input failed");

			// Add Event
			struct epoll_event in_ev;
			in_ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
			in_ev.data.fd = STDIN_FILENO;
			if (epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &in_ev) == -1)
				errno_message("Adding standard input to epoll failed");

			// Add STDIN redirection
			target_t * target = add_target(STREAM_IN);
			target->append = true;
			target->file = "[STDIN_REDIR]";
			target->fd = master[STREAM_IN];
			target->newline = false;
			target->prefix_relative = false;
			target->prefix_format = NULL;
			target->prefix_update = 1;
			target->prefix_len = 0;
		}

		if (master[STREAM_OUT] != -1) {
			struct epoll_event out_ev;
			out_ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
			out_ev.data.fd = master[STREAM_OUT];
			if (epoll_ctl(epollfd, EPOLL_CTL_ADD, master[STREAM_OUT], &out_ev) == -1)
				errno_message("Adding input/output PTY master to epoll failed");
		}

		if (master[STREAM_ERROR] != -1) {
			struct epoll_event err_ev;
			err_ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
			err_ev.data.fd = master[STREAM_ERROR];
			if (epoll_ctl(epollfd, EPOLL_CTL_ADD, master[STREAM_ERROR], &err_ev) == -1)
				errno_message("Adding error PTY master to epoll failed");
		}

		bool active = true;
		while (active) {
			struct epoll_event events[9];
			int nfds = epoll_wait(epollfd, events, 9, -1);
			if (nfds == -1) {
				if (errno == EINTR)
					continue;
				else
					errno_message("Waiting for epoll event failed");
			}

			for (int n = 0; n < nfds; ++n) {
				verbose_message("Event %d from fd %d.", events[n].events, events[n].data.fd);
				bool deregister = true;
				if (events[n].data.fd == STDIN_FILENO) {
					deregister = !epoll_handle(STDIN_FILENO, stream[STREAM_IN], stream_len[STREAM_IN], events[n].events, false);
					if (deregister)
						close(master[STREAM_IN]);
				} else if (events[n].data.fd == master[STREAM_OUT]) {
					deregister = !epoll_handle(master[STREAM_OUT], stream[STREAM_OUT], stream_len[STREAM_OUT], events[n].events, false);
				} else if (events[n].data.fd == master[STREAM_ERROR]) {
					deregister = !epoll_handle(master[STREAM_ERROR], stream[STREAM_ERROR], stream_len[STREAM_ERROR], events[n].events, false);
				} else if (events[n].data.fd == childfd) {
					verbose_message("Child process exit notification");
					active = false;
				} else {
					verbose_message("Strange event %d from fd %d.", events[n].events, events[n].data.fd);
				}
				// Deregister an FD
				if (deregister)
					epoll_ctl(epollfd, EPOLL_CTL_DEL, events[n].data.fd, NULL);

			}
		}

		int status;
		if (waitpid(pid, &status, 0) == -1 )
			errno_message("Waiting for PID %d failed", pid);

		epoll_handle(master[STREAM_OUT], stream[STREAM_OUT], stream_len[STREAM_OUT], EPOLLIN, true);
		epoll_handle(master[STREAM_ERROR], stream[STREAM_ERROR], stream_len[STREAM_ERROR], EPOLLIN, true);

		if (WIFEXITED(status)) {
			int exitcode = WEXITSTATUS(status);
			verbose_message("Exit status of PID %d was %d", pid, exitcode);
			return exitcode;
		} else if (WIFSIGNALED(status)) {
			int signal = WTERMSIG(status);
			verbose_message("Termination signal of PID %d was %d", pid, signal);
			return 128 + signal;
		} else {
			error_message("PID %d has neither exit status nor signal code", pid);
		}
	}

	return EXIT_SUCCESS;
}
