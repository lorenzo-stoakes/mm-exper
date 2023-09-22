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

int main(void)
{
	struct sockaddr_un remote;
	int sock_fd, send_fd;
	int len;

	send_fd = open("test.txt", O_RDONLY);
	if (send_fd < 0) {
		perror("open");
		return EXIT_FAILURE;
	}

	sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		perror("socket");
		return EXIT_FAILURE;
	}

	remote.sun_family = AF_UNIX;
	strcpy(remote.sun_path, socket_path);
	len = strlen(socket_path) + sizeof(remote.sun_family);

	if (connect(sock_fd, (struct sockaddr *)&remote, len) != 0) {
		perror("connect");
		return EXIT_FAILURE;
	}

	puts("client connected");

	struct custom_socket_msg custom_msg;

	strcpy(custom_msg.name, "lozzy");
	custom_msg.n = 100;

	struct msghdr msgh;
	struct iovec iov;
	union {
		struct cmsghdr cmsgh;
		char control[CMSG_SPACE(sizeof(int))];
	} control_un;

	iov.iov_base = &custom_msg;
	iov.iov_len = sizeof(custom_msg);

	msgh.msg_name = NULL;
	msgh.msg_namelen = 0;
	msgh.msg_iov = &iov;
	msgh.msg_iovlen = 1;
	msgh.msg_control = control_un.control;
	msgh.msg_controllen = sizeof(control_un.control);

	control_un.cmsgh.cmsg_len = CMSG_LEN(sizeof(int));
	control_un.cmsgh.cmsg_level = SOL_SOCKET;
	control_un.cmsgh.cmsg_type = SCM_RIGHTS;
	*((int *) CMSG_DATA(CMSG_FIRSTHDR(&msgh))) = send_fd;

	int size = sendmsg(sock_fd, &msgh, 0);
	if (size < 0) {
		perror("sendmsg");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
