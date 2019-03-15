#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 

#define IP_SIZE 20
#define BUF_SIZE 1024
#define RETRY_CNT 5

#define client_log(format, ...)   printf(format, ##__VA_ARGS__)
#define client_info(format, ...)   printf(format, ##__VA_ARGS__)
#define client_dbg(format, ...)   printf(format, ##__VA_ARGS__)

typedef struct io_rw_s {
	uint64_t address;
	uint32_t data;
}io_rw_t;

int  g_sockfd;
int  g_port   = 5000;
char g_ip[IP_SIZE];
char g_buff_in[BUF_SIZE];
char g_buff_out[BUF_SIZE];

int client_connect()
{
	struct sockaddr_in serv_addr; 
    if((g_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        client_log("\n client: Error : Could not create socket \n");
        return 1;
    } 

    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(g_port); 

    if(inet_pton(AF_INET, g_ip, &serv_addr.sin_addr)<=0)
    {
        client_log("\n client: inet_pton error occured\n");
        return 1;
    } 

    if( connect(g_sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
       client_log("\n client: Error : Connect Failed \n");
       return 1;
    } 
	client_log("client: connect success\n");
	return 0;
}

int client_disconnect()
{
	close(g_sockfd); 
}

int pingpong(char *buff_in, uint32_t len_in, char *buff_out, uint32_t *len_out)
{
	int i, ret;
	for(i=0;i<RETRY_CNT;i++)
	{
		ret = write(g_sockfd, buff_in, len_in); 
		if(ret == len_in) break;
	}
	if(RETRY_CNT == i) 
	{
		client_log("remote command-w fail\n");
		return -1;
	}
	for(i=0;i<RETRY_CNT;i++)
	{
		*len_out = read(g_sockfd, buff_out, BUF_SIZE); 
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
	int len, ret, i, len_out;
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
	int len, ret, i, len_out;
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

int main(int argc, char *argv[])
{
    int n = 0;
	uint32_t data;
    strncpy(g_ip, argv[1], IP_SIZE);

    if(argc < 2)
    {
        client_log("\n client: Usage: %s <ip of server> [Q]\n",argv[0]);
        return 1;
    } 

	client_connect();
    
	client_log("client: write\n");
    rpc_reg_write(0,0x123456);
	client_log("client: read\n");
	rpc_reg_read(0,&data);
	client_log("client: ls\n");
	rpc_system("ls");
	client_log("client: done\n");
	
	if(3 == argc)
	{
		if(argv[2][0] == 'Q') 
			rpc_quit(0);
		else
			rpc_quit(1);

	}
	else
	{
		rpc_quit(1);
	}
	
    if(n < 0)
    {
        client_log("\n client: Read error \n");
    }
    client_log("client: exit\n");
    
	client_disconnect();
    return 0;
}
