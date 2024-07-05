#include "shared.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static void* listener_thread(void *arg)
{
	long uffd = (long)arg;

	while (true) {
		struct pollfd pollfd = {
			.fd = uffd,
			.events = POLLIN,
		};
		struct uffd_msg msg = {};

		puts("poll");
		if (poll(&pollfd, 1, -1) == -1)
			pfatal("poll");

		ssize_t nread = read(uffd, &msg, sizeof(msg));
		if (nread == 0)
			fatal("EOF");

		unsigned child_uffd = handle_event(uffd, &msg);
		(void)child_uffd;
	}
}

int main(void)
{
	pthread_t thr;
	long uffd;
	int err;

	init();

	uffd = make_handler(false);
	err = pthread_create(&thr, NULL, listener_thread, (void *)uffd);
	if (err) {
		errno = err;
		pfatal("pthread_create");
	}

	char *ptr = mmap(NULL, 3*4096, PROT_READ | PROT_WRITE,
			 MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		pfatal("mmap");
	}

	register_page_range(uffd, ptr, 3);

        ptr[2*4096] = 'x';

	if (mprotect(ptr + 2*4096, 4096, PROT_NONE))
	pfatal("mprotect");

        ptr[2*4096] = 'x';

	pid_t parent_pid = getpid();
	printf("fork from pid=%d\n", parent_pid);
	pid_t pid = fork();
	if (pid == -1)
		pfatal("fork");

	// Child.
	if (pid == 0) {
		puts("hi from child");
		exit(0);
	}

	printf("child pid=%d\n", pid);

	int status;
	wait(&status);

	pthread_join(thr, NULL);

	return EXIT_SUCCESS;
}
