#include<linux/inet.h>
#include<linux/socket.h>
#include<net/sock.h>

#define IP_SIZE            20
#define BUF_SIZE           1024

typedef struct krpc_s {
//private:
	struct socket *sock;
	char buff_in[BUF_SIZE];  
	char buff_out[BUF_SIZE];
}krpc_t;

extern krpc_t g_krpc;

int krpc_reg_write(krpc_t *krpc,uint64_t address, uint32_t data);
int krpc_reg_read(krpc_t *krpc,uint64_t address, uint32_t *data);
uint32_t krpc_reg_read_ret(krpc_t *krpc,uint64_t address);
int krpc_quit(krpc_t *krpc,int only_client);
int krpc_system(krpc_t *krpc,char *cmd);
int krpc_init(krpc_t *krpc, char *ip, int port);
int krpc_deinit(krpc_t *krpc);