#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <poll.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>

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
#define DIMENSION 80

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

typedef struct {
	char name[20];
	struct sockaddr_in address;
	int flag;
	int x,y;
	int health;
	int points;
}CLIENT;

typedef struct {
	int type;
	int direction;
}CELL;

CELL matrix[DIMENSION][DIMENSION];

typedef struct {
	int row,col;
	CELL cell;
}DATA;

typedef struct {
	CLIENT clients[5];
	DATA changes[1024];
	int num_players;
	int num_changes;
	char msg[1024];
}SEND;

SEND receive;

char buffer[1024];

void getInput(char* c){
	struct pollfd mypoll = { STDIN_FILENO, POLLIN|POLLPRI };

	system ("/bin/stty raw");
	if( poll(&mypoll, 1, 50) )
	{
		scanf("%c",c);	
		return ;
	}
	system("/bin/stty cooked");
	*c = '$';
}

char get()
{
	struct termios oldattr, newattr;
	char ch;
	tcgetattr( STDIN_FILENO, &oldattr );
	newattr = oldattr;
	newattr.c_lflag &= ~( ICANON | ECHO); //knock down keybuffer
	tcsetattr( STDIN_FILENO, TCSANOW, &newattr );
	system("stty -echo"); //shell out to kill echo
	//ch = getchar();
	getInput(&ch);
	system("stty echo");
	tcsetattr( STDIN_FILENO, TCSANOW, &oldattr );
	return ch;
}

void reflect_changes(){
	int i;
	for(i=0;i<=receive.num_changes;i++){
		matrix[receive.changes[i].row][receive.changes[i].col] = receive.changes[i].cell;
	}
}

void display(){
	printf("\033[2J\033[1;1H");
	int i,j;
	printf("Nick\t\tPoints\n");
	for(i=0;i<=receive.num_players;i++){
		printf("%s\t\t%d\n",receive.clients[i].name,receive.clients[i].points);	
	}
	printf("\n\n");
	for(i=0;i<30;i++){
		for(j=0;j<DIMENSION;j++){
			if(matrix[i][j].type==P1){
			printf("%s%d",KRED,matrix[i][j].type);
			}	
			else if(matrix[i][j].type==BRICK)
			printf("%s%d",KYEL,matrix[i][j].type);
			else
			printf("%s%d",KWHT,matrix[i][j].type);
		}
		printf("\n");
	}
	i = 0, j = 0;
	while(j < 4){
		while(receive.msg[i] != '*' && receive.msg[i] != '\0'){
			printf("%c",receive.msg[i++]);
		}
		printf("\n");
		i++;
		j++;
	}
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

	addr_size = sizeof(serverAddr);
	memset(buffer,0, 1024);
	int i;

	printf("Enter the display name \n");
	buffer[0] = '*';
	scanf("%s",buffer+1);
	sendto(socketfd,&buffer,1024,0,(struct sockaddr*)&serverAddr,addr_size);
	int n;

	while(1){
		n = recvfrom(socketfd,buffer,1024,MSG_DONTWAIT,(struct sockaddr*)&serverAddr,&addr_size);
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
		if(c == '*') break;
		if(c != 'y' && c != 'n') continue;
		buffer[0] = '*';
		if(c == 'y') buffer[1] = '1';
		else buffer[1] = '2';
		buffer[2] = '\0';
		sendto(socketfd,buffer,1024,0,(struct sockaddr*)&serverAddr,addr_size);
	}
	
	n = recvfrom(socketfd,matrix,sizeof(matrix),0,(struct sockaddr*)&serverAddr,&addr_size);
	printf("%d\n",n);
	n = recvfrom(socketfd,&receive,sizeof(SEND),0,(struct sockaddr*)&serverAddr,&addr_size);
	printf("%d\n",receive.num_players);
	display();
	buffer[1] = '*';
	buffer[2] = '\0';
	while(1){ // Game starts
		buffer[0] = '$';
		//getInput(&buffer[0]);
		buffer[0] = get();
		if(buffer[0] == '*')
			break;
		if(buffer[0] == ' ' || buffer[0] == 'd' || buffer[0] == 's' || buffer[0] == 'a' || buffer[0] == 'w'){
			sendto(socketfd,buffer,1024,0,(struct sockaddr*)&serverAddr,addr_size);
		}
		while(1){
			//n = recvfrom(socketfd,&receive,sizeof(receive),MSG_DONTWAIT,(struct sockaddr*)&serverAddr,&addr_size);
			n = recvfrom(socketfd,matrix,sizeof(matrix),MSG_DONTWAIT,(struct sockaddr*)&serverAddr,&addr_size);
			if(n > 0){
				int i;
				//for(i=0;i<=receive.num_changes;i++){
				//	printf("%d %d %d\n",receive.changes[i].row,receive.changes[i].col,receive.changes[i].cell.type);
				//}
				//reflect_changes();
				display();
			}
			else break;
		}
	}
	return 0;
}
