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
#define MAX_PLAYERS 10
#define P1 1
#define P2 2
#define P3 3
#define P4 4
#define BULLET 11
#define BRICK 12
#define BLANK 0
#define DIMENSION 80
#define DIMENSION2 40
#define DIMENSION1 110
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
	int teamNo;
}CLIENT;

typedef struct {
	int type;
	int direction;
}CELL;

typedef struct {
	int row,col;
	CELL cell;
}DATA;

typedef struct {
	CLIENT clients[MAX_PLAYERS];
	CELL matrix[DIMENSION2][DIMENSION1];
	int num_players;
	char msg[1024];
	int sqNo;
}SEND;

SEND receive;

char buffer[1024];
int myIndex,isAlive,curSeq;

void getInput(char* c){
	struct pollfd mypoll = { STDIN_FILENO, POLLIN|POLLPRI };

	system ("/bin/stty raw");
	if( poll(&mypoll, 1, 50) )
	{
		scanf("%c",c);	
		return ;
	}
	system("/bin/stty cooked");
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

void print_matrix(int i, int j){

	if(receive.matrix[i][j].type==P1){
	if(receive.matrix[i][j].direction==UP)
	printf("%s%c",KRED,'^');
	else if(receive.matrix[i][j].direction==DOWN)
	printf("%s%c",KRED,'v');
	else if(receive.matrix[i][j].direction==LEFT)
	printf("%s%c",KRED,'<');
	else if(receive.matrix[i][j].direction==RIGHT)
	printf("%s%c",KRED,'>');
	}
	else if(receive.matrix[i][j].type==P2){
	if(receive.matrix[i][j].direction==UP)
	printf("%s%c",KGRN,'^');
	else if(receive.matrix[i][j].direction==DOWN)
	printf("%s%c",KGRN,'v');
	else if(receive.matrix[i][j].direction==LEFT)
	printf("%s%c",KGRN,'<');
	else if(receive.matrix[i][j].direction==RIGHT)
	printf("%s%c",KGRN,'>');
	}
	else if(receive.matrix[i][j].type==P3){
	if(receive.matrix[i][j].direction==UP)
	printf("%s%c",KMAG,'^');
	else if(receive.matrix[i][j].direction==DOWN)
	printf("%s%c",KMAG,'v');
	else if(receive.matrix[i][j].direction==LEFT)
	printf("%s%c",KMAG,'<');
	else if(receive.matrix[i][j].direction==RIGHT)
	printf("%s%c",KMAG,'>');
	}
	else if(receive.matrix[i][j].type==P4){
	if(receive.matrix[i][j].direction==UP)
	printf("%s%c",KBLU,'^');
	else if(receive.matrix[i][j].direction==DOWN)
	printf("%s%c",KBLU,'v');
	else if(receive.matrix[i][j].direction==LEFT)
	printf("%s%c",KBLU,'<');
	else if(receive.matrix[i][j].direction==RIGHT)
	printf("%s%c",KBLU,'>');
	}
	else if(receive.matrix[i][j].type==BRICK)
	printf("%s%c",KYEL,'|');
	else if(receive.matrix[i][j].type==BULLET)
	printf("%s%c",KCYN,'-');
	else
	printf("%s%c",KWHT,'*');

}

void display(){
	isAlive = 0;
	printf("\033[2J\033[1;1H");
	int i,j;
	printf("\t\t\t Time Remaining : %d\n",receive.sqNo/10);
	printf("Nick\t\tPoints\t\t\t Your health : %d\n",receive.clients[myIndex].health);
	printf("Sequence no. %d\n",curSeq);
	for(i=0;i<=receive.num_players;i++){
		printf("%s\t\t%d\n",receive.clients[i].name,receive.clients[i].points);	
	}
	printf("\n\n");
	for(i=0;i<DIMENSION2;i++){
		for(j=0;j<DIMENSION1;j++){
			if( receive.matrix[i][j].type == myIndex+1 ){
				isAlive = 1;
			}
				
			print_matrix(i,j);
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

int isValid(char c){
	if(c == ' ' || c == 'd' || c == 's' || c == 'a' || c == 'w' || c == 'p' || c == 'o') 
		return 1;
	return 0;
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
	serverAddr.sin_addr.s_addr = inet_addr("192.168.1.101");

	addr_size = sizeof(serverAddr);
	memset(buffer,0, 1024);
	int i;

	printf("Enter the display name \n");
	buffer[0] = '*';
	scanf("%s",buffer+1);
	if(buffer[1] == '*') 
		return 0;
	sendto(socketfd,&buffer,1024,0,(struct sockaddr*)&serverAddr,addr_size);
	int n;

	while(1){ // Ready and not ready block

		n = recvfrom(socketfd,buffer,1024,MSG_DONTWAIT,(struct sockaddr*)&serverAddr,&addr_size);
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
		char c = '$'; 
		getInput(&c);
		if(c == '*') break;
		if(c != 'y' && c != 'n') continue;
		buffer[0] = '*';
		if(c == 'y') buffer[1] = '1';
		else buffer[1] = '2';
		buffer[2] = '\0';
		sendto(socketfd,buffer,1024,0,(struct sockaddr*)&serverAddr,addr_size);
	}
	
	printf("here\n");
	n = recvfrom(socketfd,&receive,sizeof(SEND),0,(struct sockaddr*)&serverAddr,&addr_size);
	printf("received\n");
	if(n > 0){ // sending acknowledgment
		myIndex = receive.msg[0] - '0';
		printf("Received matrix %d %d %d\n",myIndex,receive.num_players,n);
		curSeq = receive.sqNo;
		buffer[0] = '$'; buffer[1] = myIndex + '0'; buffer[2] = '\0';
		sendto(socketfd,buffer,1024,0,(struct sockaddr*)&serverAddr,addr_size);
		printf("ACK sent %d\n",curSeq);
	}
	display();
	buffer[1] = '*';
	buffer[2] = '\0';
	isAlive = 1;
	int gameEnd = 0 , counter = 0;
	while(1){ // Game starts
		counter++;
		buffer[0] = '$';
		//buffer[0] = get();
		getInput(&buffer[0]);
		if(buffer[0] == '*')
			break;
		if(counter >= 10 && isAlive && isValid(buffer[0])){
			counter = 0;
			sendto(socketfd,buffer,1024,0,(struct sockaddr*)&serverAddr,addr_size);
		}
		while(1){
			n = recvfrom(socketfd,&receive,sizeof(SEND),MSG_DONTWAIT,(struct sockaddr*)&serverAddr,&addr_size);
			if(n > 0 && curSeq >= receive.sqNo){
				curSeq = receive.sqNo;
				printf("%s\n",receive.msg);
				if(receive.msg[0] == '*' && receive.msg[1] == '*' && receive.msg[2] == '*'){ // level 1 ends
					printf("level1 ends!!\n");
					gameEnd = 1;
					buffer[0] = '$'; buffer[1] = '0' + myIndex; buffer[2] = '\0';
					sendto(socketfd,buffer,1024,0,(struct sockaddr*)&serverAddr,addr_size);
					break;
				}
				display();
			}
			else break;
		}
		if(gameEnd) break;
	}
	memset(buffer,0,sizeof buffer);
	printf("2");
	while(buffer[0] != '$'){
		printf("2");
		n = recvfrom(socketfd,buffer,1024,0,(struct sockaddr*)&serverAddr,&addr_size);
	}
	n = recvfrom(socketfd,buffer,1024,MSG_DONTWAIT,(struct sockaddr*)&serverAddr,&addr_size);
	char team = '$';
	printf("%s",KWHT);
	printf("%s\n",buffer);
	while(1){ // level 2 starting 
		int flag = 0;
		if(n > 0){
			if(buffer[0] == '$'){
				printf("\n\t\t\t\tLevel 2 is about to start !! \n");
				break;
			}
			flag = 1;
			printf("\t\t\t\tPlayers currently connected \n\n");
			i = 0;	
			printf("%s\n",buffer);
			while(buffer[i]){
				if(buffer[i] == '*')									
					printf("\n\t\t\t\t");
				else if(buffer[i] == '$'){
					printf("\t");
					if(buffer[i+1] == 1) printf("Ready\t");
					else printf("Not Ready\t");
					printf("%c Team",buffer[i+2]);
					i += 2;
				}
				else printf("%c",buffer[i]);
				i++;
			}
			printf("\n\n");
		}
		if(flag){
			printf("\t\t\t\tWhich team do you want to join ?? (range 1-9).\n\t\t\t\t Are you ready for the game ?? (y or n) : format ex -> y2 \n");
		}
		char c = '$'; 
		getInput(&c);
		if(team == '*') break;
		if(c != 'y' && c != 'n'){
			n = recvfrom(socketfd,buffer,1024,MSG_DONTWAIT,(struct sockaddr*)&serverAddr,&addr_size);
			continue;
		}
		scanf("%c",&team);
		buffer[0] = '*';
		if(c == 'y') buffer[1] = '1';
		else buffer[1] = '2';
		buffer[2] = team;
		buffer[3] = '\0';
		sendto(socketfd,buffer,1024,0,(struct sockaddr*)&serverAddr,addr_size);
		n = recvfrom(socketfd,buffer,1024,MSG_DONTWAIT,(struct sockaddr*)&serverAddr,&addr_size);
	}
	return 0;
}
