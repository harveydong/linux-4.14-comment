#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h> 


#define REG_BASE       		0 //0xA0000000
#define REG_SIZE       		0x50000 //0x05000000
#define MAP_MASK 			(REG_SIZE - 1)
#define REG(offset)         (*((volatile uint32_t*) (g_virt_addr + offset)))
#define BUF_SIZE            1024

#define server_log(format, ...)   printf(format, ##__VA_ARGS__)
#define server_info(format, ...)   printf(format, ##__VA_ARGS__)

#define server_dbg(format, ...)   printf(format, ##__VA_ARGS__)


typedef struct io_rw_s {
	uint64_t address;
	uint32_t data;
}io_rw_t;

uint8_t *g_virt_addr;



uint8_t *reg_map(int *fd, off_t address, uint32_t size, void **map_base)
{
	uint8_t *virt_addr; 
	unsigned long read_result, writeval;
	
    //The O_SYNC option is recommended to avoid Linux to cache the content of /dev/mem and delay any modification done in this file. 
    if((*fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1)
	{
		server_log("open /dev/mem fail\n");
		fflush(stdout);
	}
    
    /* Map one page */
    *map_base = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, *fd, address & ~MAP_MASK);
    if(*map_base == (void *) -1)
	{
		server_log("/dev/mem map address fail\n");
		fflush(stdout);
	}
    
    
    virt_addr = *map_base + (address & MAP_MASK);
	server_log("/dev/mem map %lx to %p, (base=%p)\n", address, virt_addr, map_base);
	fflush(stdout);
	return virt_addr;
}

void reg_umap(int fd, uint32_t size, void *map_base)
{
	if(munmap(map_base, size) == -1)
		server_log("munmap fail\n");
	close(fd);
}

int main(int argc, char *argv[])
{
    int listenfd = 0, connfd = 0, rx_len = 0;
    struct sockaddr_in serv_addr; 
	int server_loop = 1;
	int client_loop = 1;

    char sendBuff[BUF_SIZE];
    char recvBuff[BUF_SIZE];
    time_t ticks; 

	int fd;
	void *map_base;
	g_virt_addr = reg_map(&fd, REG_BASE, REG_SIZE, &map_base);
	

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));
    memset(sendBuff, '0', sizeof(sendBuff)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(5000); 

    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)); 

    listen(listenfd, 10); 

    while(server_loop)
    {
	server_log("server: wait client connet\n");
        connfd = accept(listenfd, (struct sockaddr*)NULL, NULL); 
	server_log("server: connfd =%x\n", connfd);

        ticks = time(NULL);
        snprintf(sendBuff, sizeof(sendBuff), "%.24s\r\n", ctime(&ticks));
        write(connfd, sendBuff, strlen(sendBuff)); 


		server_log("server: wait data\n");
		client_loop = 1;
		while (client_loop)
		{
			rx_len = read(connfd, recvBuff, BUF_SIZE-1);
			recvBuff[rx_len] = 0;
			server_log("server: client request: %c :",recvBuff[0]);
			//if(rx_len<=0) break;
			switch(recvBuff[0])
			{
				case 'q':
					server_log("client request close connection\n");
					client_loop = 0;
					write(connfd, recvBuff, rx_len);//send recvBuff data back
					break;
				case 'Q':
					server_log("client request server to exit\n");
					client_loop = 0;
					server_loop = 0;
					write(connfd, recvBuff, rx_len);//send recvBuff data back
					break;
				case 'w':
				{
					io_rw_t *io_rw = (io_rw_t*) &recvBuff[1];					
					REG(io_rw->address - REG_BASE) = io_rw->data;
					server_log("w %8.8lx %8.8x\n",io_rw->address,io_rw->data);
					write(connfd, recvBuff, rx_len);//send recvBuff data back
				}
					break;
				case 'r':
				{
					io_rw_t *io_rw = (io_rw_t*) &recvBuff[1];
					io_rw->data = REG(io_rw->address - REG_BASE);					
					server_log("w %8.8lx %8.8x\n",io_rw->address,io_rw->data);
					write(connfd, recvBuff, rx_len);//send recvBuff data back
				}
					break;
				case 's':
					server_log("server: system(\"%s\")\n",recvBuff+1);
					system(recvBuff+1);
					write(connfd, recvBuff, rx_len);//send recvBuff data back
					break;
				default:
					client_loop = 0;
					server_log("server: cmd unknow\n");
					break;
			}
		} 
		close(connfd);
     }
    close(listenfd);
	reg_umap(fd, REG_SIZE, map_base);
    server_log("server: exit\n");
}
