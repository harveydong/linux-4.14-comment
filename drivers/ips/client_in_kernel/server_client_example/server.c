#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h> 

int main(int argc, char *argv[])
{
    int listenfd = 0, connfd = 0, n = 0;
    struct sockaddr_in serv_addr; 
	int server_loop = 1;

    char sendBuff[1025];
    char recvBuff[1025];
    time_t ticks; 

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
	printf("server: wait client connet\n");
        connfd = accept(listenfd, (struct sockaddr*)NULL, NULL); 
	printf("server: connfd =%x\n", connfd);

        ticks = time(NULL);
        snprintf(sendBuff, sizeof(sendBuff), "%.24s\r\n", ctime(&ticks));
        write(connfd, sendBuff, strlen(sendBuff)); 


		printf("server: wait data\n");
		while ( (n = read(connfd, recvBuff, sizeof(recvBuff)-1)) > 0)
		{
			recvBuff[n] = 0;
			printf("server: get command from client:");
			if(fputs(recvBuff, stdout) == EOF)
			{
				printf("\n server: Error : Fputs error\n");
			}
			printf("\n");
			if(recvBuff[0]=='Q') 
			{
				printf("server: Bye bye\n");
				server_loop = 0;
				close(connfd);
				break;
			}
			close(connfd);
			sleep(1);			
		} 
     }
    close(listenfd);
    printf("server: exit\n");
}
