#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
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
    struct sockaddr_in address;
    //unsigned long address;
    //char address[20];
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

SEND sends;
int num_players = -1;
char buffer[1024];

int check_player(struct sockaddr_in clientAddr)
{//inet_ntoa(clientAddress.sin_addr)
	for(int i=0;i<=num_players;i++)
		//if(strcmp((sends.clients[i]).address,clientAddr.sin_addr)==0)
		if((sends.clients[i].address).sin_addr.s_addr == clientAddr.sin_addr.s_addr)
			return i;
	return -1;
}

void make_player(struct sockaddr_in clientAddr)
{
	num_players++;
	(sends.clients[num_players]).address = clientAddr;
	(sends.clients[num_players]).flag = 2;
	strcpy((sends.clients[num_players]).name,buffer+1);
	//sends.clients[num_players].address = clientAddr.sin_addr.s_addr;
}

void get_players_list()
{
	int ctr = 0;
	for(int i=0;i<=num_players;i++)
	{
		buffer[ctr] = '*';
		strcpy(buffer+ctr+1,(sends.clients[i]).name);
		ctr+=strlen((sends.clients[i]).name)+1;
		buffer[ctr] = '$';
		ctr++;
		buffer[ctr] = (sends.clients[i]).flag;
		ctr++;
	}
	buffer[ctr] = '\0';
}

int all_ready()
{
	for(int i=0;i<=num_players;i++)
	{
		if((sends.clients[i]).flag==2)
			return 0;
	}
	return 1;
}

int main()
{	
	int socketfd,n;
	struct sockaddr_in serverAddr,clientAddr;
	socklen_t addr_size;

	socketfd = socket(PF_INET,SOCK_DGRAM,0);
	memset((char *)&serverAddr,0,sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(9000);
	serverAddr.sin_addr.s_addr = inet_addr("172.17.47.15");
	bind(socketfd,(struct sockaddr *)&serverAddr,sizeof(serverAddr));
	addr_size = sizeof(serverAddr);

	memset(buffer,'\0',sizeof(buffer));
	//memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
	
	

	while(1)
	{
		n = recvfrom(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,&addr_size);
		printf("%s\n",buffer);
		int t = check_player(clientAddr);
		printf("%d\n",t);
		if(t==-1)
		{
			if(num_players==4)
			{
				strcpy(buffer,"Four Players Already Connected... Please try in next game\n"); 
				sendto(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,addr_size);
			}
			else
			{
				make_player(clientAddr);
				get_players_list();
				printf("%s\n",buffer);
				//sendsing to all players
				for(int i=0;i<=num_players;i++)
				{
					printf("Name1 - %d\n",num_players);
					printf("Name1 - %s\n",(sends.clients[i]).name);
					sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
				}
			}			
		}
		else
		{	
			if(!(strcmp(buffer,"*1")==0 || strcmp(buffer,"*2")==0))
			{
				strcpy(buffer,"Invalid Input\n"); 
				sendto(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,addr_size);
			}
			(sends.clients[t]).flag = buffer[1]-'0';
			get_players_list();
			//sendsing to all players
			for(int i=0;i<=num_players;i++)
			{
				printf("Name2 - %d\n",num_players);
				printf("Name2 - %s\n",(sends.clients[i]).name);
				sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
			}
		}

		if(t=all_ready())
		{
			printf("%d\n",t);
			strcpy(buffer,"$"); 
			for(int i=0;i<=num_players;i++)
				sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
			break;
		}	
		
	}
	printf("out\n");
	return 0;
}