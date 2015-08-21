#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <strings.h>
#include <stdlib.h>
#include <linux/if.h>
#include <linux/if_tun.h>

int main(int argc, char *argv[])
{
	struct sockaddr_in addr;
	struct ifreq ifr;
	int srv_port, clnt_port;
	int sk, tun, fd;
	struct pollfd fds[2];
	char *dev_name, *ns_path;

	if (argc < 6) {
		fprintf(stderr, "%s dst_addr src_port dst_port ns_path dev_name\n", argv[0]);
		return 1;
	}

	srv_port  = atoi(argv[2]);
	clnt_port = atoi(argv[3]);
	ns_path   = argv[4];
	dev_name  = argv[5];

	sk = socket(AF_INET, SOCK_DGRAM, 0);
	if (sk < 0) {
		perror("Unable to create a socket");
		return 1;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("0.0.0.0");
	addr.sin_port = htons(srv_port);
	if (bind(sk,(struct sockaddr *)&addr,sizeof(addr))) {
		perror("Unable to bind a transport socket");
		return 1;
	}

	bzero(&addr,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(argv[1]);
	addr.sin_port = htons(clnt_port);
	if (connect(sk, (struct sockaddr *)&addr,sizeof(addr))) {
		perror("Unable to connect");
		return 1;
	}

	fd = open(ns_path, O_RDONLY);
	if (fd < 0) {
		perror("Unable to open the netns");
		return 1;
	}
	if (setns(fd, CLONE_NEWNET)) {
		perror("Unable to create a new network device");
		close(fd);
		return 1;
	}
	close(fd);

	tun = open("/dev/net/tun", O_RDWR);
	if (tun < 0) {
		perror("Unable to open /dev/net/tun");
		return 1;
	}
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TUN;
	strncpy(ifr.ifr_name, dev_name, IFNAMSIZ);
	if (ioctl(tun, TUNSETIFF, (void *) &ifr)) {
		perror("Unable to create a net device");
		return 1;
	}

	fds[0].fd = tun;
	fds[0].events = POLLIN;
	fds[1].fd = sk;
	fds[1].events = POLLIN;

	while (1) {
		char buf[1 << 20];
		int ret;

		if (poll(fds, 2, -1) < 0) {
			perror("poll");
			return 1;
		}
		if (fds[0].revents == POLLIN) {
			ret = read(tun, buf, sizeof(buf));
			if (ret < 0) {
				perror("Unable to read from the tun device");
				return 1;
			}
			if (write(sk, buf, ret) < 0) {
				perror("Unable to write into the transport socket");
				return 1;
			}
		}
		if (fds[1].revents == POLLIN) {
			ret = read(sk, buf, sizeof(buf));
			if (ret < 0) {
				perror("Unable to read from the transport device");
				return 1;
			}
			if (write(tun, buf, ret) < 0) {
				perror("Unable to write into the tun device");
				return 1;
			}
		}
	}

	return 0;
}
