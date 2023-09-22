#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>

static const char *socket_path = "/home/lorenzo/test.socket";

#define MAX_NAME_SIZE (20)

struct custom_socket_msg {
	char name[MAX_NAME_SIZE];
	int n;
};

static void output_file(int fd)
{
	char buf[512];
	int num_bytes;

	num_bytes = read(fd, &buf, sizeof(buf));
	if (num_bytes < 0) {
		perror("read");
		exit(1);
	}
	buf[num_bytes] = '\0';

	printf("OUTPUT:\n%s", buf);

	close(fd);
}

int main(void)
{
	struct sockaddr_un local, remote;
	int sock_fd;
	int len;

	sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		perror("socket");
		return EXIT_FAILURE;
	}

	// Delete socket file if already exists.
	unlink(socket_path);

	local.sun_family = AF_UNIX;
	strcpy(local.sun_path, socket_path);
	len = strlen(socket_path) + sizeof(local.sun_family);

	if (bind(sock_fd, (struct sockaddr *)&local, len) != 0) {
		perror("bind");
		return EXIT_FAILURE;
	}

	if (listen(sock_fd, 10)) {
		perror("listen");
		return EXIT_FAILURE;
	}

	puts("set up.");

	while (true) {
		int accepted_fd;
		unsigned int sock_len;
		puts("Waiting for connection...");
		int recv_fd;

		accepted_fd = accept(sock_fd, (struct sockaddr *)&remote, &sock_len);
		if (accepted_fd < 0) {
			perror("accept");
			return EXIT_FAILURE;
		}

		puts("Server connected.");

		struct msghdr msgh;
		struct iovec iov;
		union {
			struct cmsghdr cmsgh;
			char control[CMSG_SPACE(sizeof(int))];
		} control_un;
		struct cmsghdr *cmsgh;
		struct custom_socket_msg custom_msg;

		iov.iov_base = &custom_msg;
		iov.iov_len = sizeof(custom_msg);

		msgh.msg_name = NULL;
		msgh.msg_namelen = 0;
		msgh.msg_iov = &iov;
		msgh.msg_iovlen = 1;
		msgh.msg_control = control_un.control;
		msgh.msg_controllen = sizeof(control_un.control);

		int size = recvmsg(accepted_fd, &msgh, 0);
		if (size < 0) {
			perror("recvmsg");
			return EXIT_FAILURE;
		}

		if (size != sizeof(custom_msg)) {
			fprintf(stderr, "Unexpected size %d\n", size);
			return EXIT_FAILURE;
		}

		printf("CONNECT: name=[%s], n=[%d]\n", custom_msg.name, custom_msg.n);

		cmsgh = CMSG_FIRSTHDR(&msgh);
		if (!cmsgh) {
			printf("<no fd>\n");
			continue;
		}

		if (cmsgh->cmsg_level != SOL_SOCKET ||
		    cmsgh->cmsg_type != SCM_RIGHTS) {
			fprintf(stderr,
				"Invalid level/rights %d/%d\n", cmsgh->cmsg_level,
				cmsgh->cmsg_type);
			return EXIT_FAILURE;
		}

		recv_fd = *((int *) CMSG_DATA(cmsgh));
		output_file(recv_fd);
	}

	return EXIT_SUCCESS;
}
