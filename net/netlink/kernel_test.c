#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <linux/sched.h>

#define NETLINK_TEST 21
struct sock *nl_sk = NULL;

static void nl_data_ready(struct sk_buff *_skb)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	u32 pid;
	int rc;
	int len = NLMSG_SPACE(100);
	char str[100];

	printk("netlink: data is ready to read.\n");
	skb = skb_get(_skb);
	
	if (skb->len >= NLMSG_SPACE(0)) {
		nlh = nlmsg_hdr(skb);
		printk("netlink: recv: %s.\n", (char*)NLMSG_DATA(nlh));
	
		memcpy(str, NLMSG_DATA(nlh), sizeof(str));
		pid = nlh->nlmsg_pid;
		printk("netlink: sender pid is %d\n", pid);
		kfree_skb(skb);
		
		skb = alloc_skb(len, GFP_ATOMIC);
		if (!skb) {
			printk("netlink: allocted skb failed\n");
			return;
		}

		nlh = nlmsg_put(skb, 0, 0,0,20,0);
		NETLINK_CB(skb).pid = 0;
		memcpy(NLMSG_DATA(nlh), str, sizeof(str));
		rc = netlink_unicast(nl_sk, skb, pid, MSG_DONTWAIT);
		if (rc < 0) {
			printk("netlink: cat not unicast skb(%d)\n",rc);
		}

		printk("netlink: Send is ok from kernel\n");
	}

	return;
}

static int init_module(void)
{
	nl_sk = netlink_kernel_create(&init_net, NETLINK_TEST, 0, nl_data_ready, NULL, THIS_MODULE);
	if (!nl_sk) {
		printk("net link: cannot create kernel netlink socket.\n");
		return -EIO;
	}
	printk("net link: netlink create socket ok\n");	
	return 0;
}

static void exit_module()
{
	if (nl_sk != NULL)
		sock_release(nl_sk->sk_socket);

	printk("net link :remove is ok\n");
}

module_init(init_module);
module_exit(exit_module);
MODULE_LICENSE("GPL");

