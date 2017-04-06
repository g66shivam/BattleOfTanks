#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <poll.h>
#include <unistd.h>
#include <stdlib.h>

#define UP 1
#define DOWN 2
#define RIGHT 3
#define LEFT 4
#define MAX_PLAYERS 4
#define P1 1
#define P2 2
#define P3 3
#define P4 4
#define BULLET 5
#define BRICK 6
#define BLANK 0

typedef struct {
	char name[20];
	//IP address
	int flag;
	int x,y;
	int health;
	int points;
}CLIENT;

typedef struct {
	int type;
	int direction;
}CELL;

CELL matrix[100][100];

typedef struct {
	int row,col;
	CELL cell;
}DATA;

typedef struct {
	CLIENT clients[5];
	DATA changes[1024];
}SEND;

SEND snd;

char buffer[1024];

void getInput(char* c){
	struct pollfd mypoll = { STDIN_FILENO, POLLIN|POLLPRI };

	system ("/bin/stty raw");
	if( poll(&mypoll, 1, 500) )
	{
    	scanf("%c",c);	
		return ;
	}
	system("/bin/stty cooked");
	*c = '$';
}

int main()
{
	int socketfd,nBytes;

	struct sockaddr_in serverAddr;
	socklen_t addr_size;
	socketfd = socket(PF_INET,SOCK_DGRAM,0);

	memset((char *)&serverAddr,0,sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(9000);
	serverAddr.sin_addr.s_addr = inet_addr("172.17.47.15");
	//memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
	//bind(socketfd,(stuct sockaddr *)&serverAddr,sizeof(serverAddr));

	addr_size = sizeof(serverAddr);
	memset(buffer,0, 1024);
	int i;

	printf("Enter the display name \n");
	buffer[0] = '*';
	scanf("%s",buffer+1);
	sendto(socketfd,&buffer,1024,0,(struct sockaddr*)&serverAddr,addr_size);

	while(1){
		int n = recvfrom(socketfd,buffer,1024,MSG_DONTWAIT,(struct sockaddr*)&serverAddr,&addr_size);
		//printf("%s %d \n",buffer,n);
		int flag = 0;
		if(n > 0){
			flag = 1;
			if(buffer[0] == '$'){
				printf("\t\t\t\t\t\tGame will start in few seconds \n\n");
				break;
			}
			printf("\t\t\t\t\t\tPlayers currently connected \n\n");
			i = 0;
			while(buffer[i]){
				switch(buffer[i]){
					case '*' : 
						printf("\n\t\t\t\t\t\t"); break;
					case '$' :
						printf("\t"); break;
					case 1 :
						printf("Ready"); break;
					case 2 :
						printf("Not Ready"); break;
					default :
						printf("%c",buffer[i]);
				}
				i++;
			}
		}
		if(flag) printf("\t\t\t\t\t\n Ready for the game? (y/n)");
		fflush(stdout);
		char c; 
		getInput(&c);
		//printf("%c\n",c);
		if(c != 'y' && c != 'n') continue;
		buffer[0] = '*';
		if(c == 'y') buffer[1] = '1';
		else buffer[1] = '2';
		buffer[2] = '\0';
		sendto(socketfd,&buffer,1024,0,(struct sockaddr*)&serverAddr,addr_size);
		printf("sent\n");
	}
	return 0;
}
