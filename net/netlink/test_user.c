#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <linux/netlink.h>
#include <linux/socket.h>
#include <asm/types.h>

#define NETLINK_TEST 21
int sock_fd;
struct msghdr msg;
struct sockaddr_nl src_addr, dst_addr;
#define MAX_LEN 100
struct nlmsghdr *nlh = NULL;
struct iovec iov;

void main()
{
	
	sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_TEST);
	memset(&msg, 0, sizeof(msg));
	memset(&src_addr, 0, sizeof(src_addr));

	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid();
	src_addr.nl_groups = 0;
	bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr));
	
	memset(&dst_addr, 0, sizeof(dst_addr));
	dst_addr.nl_family = AF_NETLINK;
	dst_addr.nl_pid = 0;
	dst_addr.nl_groups = 0;

	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_LEN));
	
	nlh->nlmsg_len = NLMSG_SPACE(MAX_LEN);
	nlh->nlmsg_pid = getpid();//这里发送给自己了.
	nlh->nlmsg_flags = 0;

	strcpy(NLMSG_DATA(nlh), "This is from user space netlink message!");
	iov.iov_base = (void*)nlh;
	iov.iov_len = nlh->nlmsg_len;
	
	msg.msg_name = (void*)&dst_addr;
	msg.msg_namelen = sizeof(dst_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	printf("sending message.....\n");
	sendmsg(sock_fd, &msg, 0);

	memset(nlh, 0, NLMSG_SPACE(MAX_LEN));
	printf("waiting message....\n");
	recvmsg(sock_fd, &msg, 0);
	printf("Receied message from kernel: %s\n", NLMSG_DATA(nlh));
	close(sock_fd);

}
