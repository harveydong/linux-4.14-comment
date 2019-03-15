#include<linux/in.h>
#include<linux/inet.h>
#include<linux/socket.h>
#include<net/sock.h>
#include<linux/init.h>
#include<linux/module.h>

//======================================================================
#define IP_SIZE 20
#define BUF_SIZE 1024
#define RETRY_CNT 5

#define client_log(format, ...)   printk(format, ##__VA_ARGS__)
#define client_info(format, ...)   printk(format, ##__VA_ARGS__)
#define client_dbg(format, ...)   printk(format, ##__VA_ARGS__)

typedef struct io_rw_s {
	uint64_t address;
	uint32_t data;
}io_rw_t;

struct socket *g_sock;
int  g_port   = 5000;
char g_ip[IP_SIZE] = "192.168.1.202";
char g_buff_in[BUF_SIZE];
char g_buff_out[BUF_SIZE];

int client_connect(void)
{
	int ret = 0;
	struct sockaddr_in serv_addr; 
	ret = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, 0, &g_sock);
    if(ret < 0)
    {
        client_log("\n client: Error : Could not create socket \n");
        return 1;
    } 

    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(g_port); 
	serv_addr.sin_addr.s_addr = in_aton(g_ip);
	
	ret = g_sock->ops->connect(g_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr), 0);
    if(ret != 0)
    {
       client_log("\n client: Error : Connect Failed \n");
       return 1;
    } 
	client_log("client: connect success\n");
	return 0;
}

int client_disconnect(void)
{
	kernel_sock_shutdown(g_sock, SHUT_RDWR);
	sock_release(g_sock);
}

int pingpong(char *buff_in, uint32_t len_in, char *buff_out, uint32_t *len_out)
{
	struct kvec send_vec, recv_vec;
    struct msghdr send_msg, recv_msg;
	int i, ret;
	for(i=0;i<RETRY_CNT;i++)
	{
		memset(&send_msg, 0, sizeof(send_msg));
		memset(&send_vec, 0, sizeof(send_vec));
		send_vec.iov_base = buff_in;
		send_vec.iov_len = len_in;
		ret = kernel_sendmsg(g_sock, &send_msg, &send_vec, 1, len_in);
		if(ret == len_in) break;
	}
	if(RETRY_CNT == i) 
	{
		client_log("remote command-w fail\n");
		return -1;
	}
	for(i=0;i<RETRY_CNT;i++)
	{
		memset(&recv_vec, 0, sizeof(recv_vec));
		memset(&recv_msg, 0, sizeof(recv_msg));
		recv_vec.iov_base = buff_out;
		recv_vec.iov_len = BUF_SIZE;
		*len_out = kernel_recvmsg(g_sock, &recv_msg, &recv_vec, 1, BUF_SIZE, 0);
		if(*len_out) break;
	}
	if(RETRY_CNT == i) 
	{
		client_log("remote command-r fail\n");
		return -1;
	}
	return 0;
}

int rpc_reg_write(uint64_t address, uint32_t data)
{
	int len, ret, len_out;
	io_rw_t *io_rw = (io_rw_t*)(g_buff_in+1);
	
	g_buff_in[0] = 'w';
	io_rw->address = address;
	io_rw->data = data;
	len = sizeof(io_rw_t)+1;
	ret = pingpong(g_buff_in, len, g_buff_out, &len_out);

	return ret;
}

int rpc_reg_read(uint64_t address, uint32_t *data)
{
	int len, ret, len_out;
	io_rw_t *io_rw = (io_rw_t*)(g_buff_in+1);
	
	g_buff_in[0] = 'r';
	io_rw->address = address;	
	len = sizeof(io_rw_t)+1;
	ret = pingpong(g_buff_in, len, g_buff_out, &len_out);
	*data = io_rw->data;
	
	return ret;
}

int rpc_quit(int only_client)
{
	int len, ret, len_out;
	if(only_client)
		g_buff_in[0] = 'q';
	else
		g_buff_in[0] = 'Q';
	len = 2;
	ret = pingpong(g_buff_in, len, g_buff_out, &len_out);
	return ret;
}

int rpc_system(char *cmd)
{
	int len, ret, len_out;
	g_buff_in[0] = 's';
	snprintf(g_buff_in+1,strlen(cmd)+1,"%s",cmd);
	len = strlen(cmd)+1;
	client_log("client: send command %s\n",g_buff_in);
	ret = pingpong(g_buff_in, len, g_buff_out, &len_out);
	return ret;
}

int test_main(void)
{
    int n = 0;
	uint32_t data;

	client_connect();
    
	client_log("client: write\n");
    rpc_reg_write(0,0x123456);
	client_log("client: read\n");
	rpc_reg_read(0,&data);
	client_log("client: ls\n");
	rpc_system("ls");
	client_log("client: done\n");
	
	rpc_quit(1);
	
    if(n < 0)
    {
        client_log("\n client: Read error \n");
    }
    client_log("client: exit\n");
    
	client_disconnect();
    return 0;
}
//======================================================================
#define BUFFER_SIZE 1024
int connect_send_recv(void){
    struct socket *sock;
    struct sockaddr_in s_addr;
    unsigned short port_num = 5000;
    int ret = 0;
    char *send_buf = NULL;
    char *recv_buf = NULL;
    struct kvec send_vec, recv_vec;
    struct msghdr send_msg, recv_msg;

    /* kmalloc a send buffer*/
    send_buf = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (send_buf == NULL) {
        printk("client: send_buf kmalloc error!\n");
        return -1;
    }

    /* kmalloc a receive buffer*/
    recv_buf = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if(recv_buf == NULL){
        printk("client: recv_buf kmalloc error!\n");
        return -1;
    }

    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(port_num);

    s_addr.sin_addr.s_addr = in_aton("192.168.1.101");
    sock = (struct socket *)kmalloc(sizeof(struct socket), GFP_KERNEL);

    // 创建一个sock, &init_net是默认网络命名空间
    ret = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, 0, &sock);
    if (ret < 0) {
        printk("client:socket create error!\n");
        return ret;
    }
    printk("client: socket create ok!\n");

    //连接
    ret = sock->ops->connect(sock, (struct sockaddr *)&s_addr, sizeof(s_addr), 0);
    if (ret != 0) {
        printk("client: connect error!\n");
        return ret;
    }
    printk("client: connect ok!\n");

    memset(send_buf, 'a', BUFFER_SIZE);

    memset(&send_msg, 0, sizeof(send_msg));
    memset(&send_vec, 0, sizeof(send_vec));

    send_vec.iov_base = send_buf;
    send_vec.iov_len = BUFFER_SIZE;

    // 发送数据
    ret = kernel_sendmsg(sock, &send_msg, &send_vec, 1, BUFFER_SIZE);
    if (ret < 0) {
        printk("client: kernel_sendmsg error!\n");
        return ret;
    } else if(ret != BUFFER_SIZE){
        printk("client: ret!=BUFFER_SIZE");
    }
    printk("client: send ok!\n");

    memset(recv_buf, 0, BUFFER_SIZE);

    memset(&recv_vec, 0, sizeof(recv_vec));
    memset(&recv_msg, 0, sizeof(recv_msg));

    recv_vec.iov_base = recv_buf;
    recv_vec.iov_len = BUFFER_SIZE;

    // 接收数据
    ret = kernel_recvmsg(sock, &recv_msg, &recv_vec, 1, BUFFER_SIZE, 0);
    printk("client: received message:\n %s\n", recv_buf);

    // 关闭连接
    kernel_sock_shutdown(sock, SHUT_RDWR);
    sock_release(sock);

    return 0;
}


//======================================================================
static int client_example_init(void){
    printk("client: init\n");
    //connect_send_recv();
	test_main();
	return 0;
}

static void client_example_exit(void){
    printk("client: exit!\n");
}

module_init(client_example_init);
module_exit(client_example_exit);
MODULE_LICENSE("GPL");