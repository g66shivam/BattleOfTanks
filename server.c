#include <stdio.h>
#include <malloc.h>
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
#define MAX_PLAYERS 10
#define P1 1
#define P2 2
#define P3 3
#define P4 4
#define BULLETS 11
#define BRICK 12
#define BLANK 0
#define DIMENSION 80
#define MAZES 10
#define DIMENSION1 110
#define DIMENSION2 40

int dx[4] = {0,-1,0,1};// left,up, right, down
int dy[4] = {-1,0,1,0};
int next_spawn[12];
int global_changes = -1;

int max(int a, int b)
{
	if(a>b)
		return a;
	return b;
}

typedef struct {
    char name[20];
    struct sockaddr_in address;
    int flag;
    int x,y;
    int health;
    int points;
    int teamno;
}CLIENT;

typedef struct {
    int type;
    int direction;
}CELL;

//CELL matrix[DIMENSION][DIMENSION];
CELL mazedata[MAZES][DIMENSION2][DIMENSION1];

typedef struct {
    int row,col;
    CELL cell;
}DATA;

typedef struct {
    CLIENT clients[MAX_PLAYERS];
    CELL matrix[DIMENSION2][DIMENSION1];
    int num_players;
    char msg[1024];
    int sqno;
}SEND;

typedef struct {
	int x;
	int y;
	int dir;
	int p_num;
	int to_delete;
	struct BULLET *next;
}BULLET;

BULLET *bullet = NULL;
SEND sends = {.num_players=-1,.msg=""};
char buffer[1024];
int received[MAX_PLAYERS]={0};

BULLET* make_bullet(int x,int y,int dir,int player)
{
	BULLET* temp = (BULLET*)malloc(sizeof(BULLET));
	temp->x = x;
	temp->y = y;
	temp->dir = dir;
	temp->to_delete = 0;
	temp->p_num = player;
	temp->next = NULL;
	return temp;
}

void del_bullet()
{
	BULLET *temp;
	BULLET *head=NULL;

	while(bullet!=NULL)
	{
		if(bullet->to_delete==0)
		{
			if(head==NULL)
			{
				head = bullet;
				temp = head;
			}
			else
			{
				temp->next = bullet;
				temp = temp->next;		
			}
		}
		else
			printf("DEleting this bullet\n");
		bullet = bullet->next;
	}
	bullet = head;
}

int check_player(struct sockaddr_in clientAddr)
{
	for(int i=0;i<=sends.num_players;i++)
		if((sends.clients[i].address).sin_addr.s_addr == clientAddr.sin_addr.s_addr)
			return i;
	return -1;
}

void make_player(struct sockaddr_in clientAddr)
{
	sends.num_players++;
	(sends.clients[sends.num_players]).address = clientAddr;
	(sends.clients[sends.num_players]).flag = 2;
	strcpy((sends.clients[sends.num_players]).name,buffer+1);
	(sends.clients[sends.num_players]).health = 100;
	(sends.clients[sends.num_players]).points = 0;
	(sends.clients[sends.num_players]).teamno = 0;
	//sends.clients[num_players].address = clientAddr.sin_addr.s_addr;
}

void respawn()
{
	int i;
	for (i = 0; i < sends.num_players; ++i)
	{
		if(next_spawn[i] == sends.sqno)
		{
			sends.matrix[sends.clients[i].x][sends.clients[i].y].type = i+1;
			sends.matrix[sends.clients[i].x][sends.clients[i].y].direction = DOWN;
			next_spawn[i] = -1; 
		}
	}
}

void get_players_list()
{
	int ctr = 0;
	for(int i=0;i<=sends.num_players;i++)
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

void get_team_list()
{
	memset(buffer,'\0',sizeof(buffer));
	int ctr = 0;
	for(int i=0;i<=sends.num_players;i++)
	{
		buffer[ctr] = '*';
		strcpy(buffer+ctr+1,(sends.clients[i]).name);
		ctr+=strlen((sends.clients[i]).name)+1;
		buffer[ctr] = '$';
		ctr++;
		buffer[ctr] = (sends.clients[i]).flag;
		ctr++;
		buffer[ctr] = (sends.clients[i]).teamno+'0';
		ctr++;
	}
	buffer[ctr] = '\0';
}

int all_ready()
{
	for(int i=0;i<=sends.num_players;i++)
	{
		if((sends.clients[i]).flag==2)
			return 0;
	}
	return 1;
}

int all_received()
{
	for(int i=0;i<=sends.num_players;i++)
	{
		if(received[i]==0)
			return 0;
	}
	return 1;
}


void get_Pos(int *x,int *y)// dummy_function,modify this function to get a random position
{
	do
	{
		*x = rand()%DIMENSION2; 
		*y = rand()%DIMENSION1; 
	}while(sends.matrix[*x][*y].type!=BLANK);
}

int map_char_to_idx(char c)
{
	if(c=='a')
		return 0;
	if(c=='w')
		return 1;
	if(c=='d')
		return 2;
	if(c=='s')
		return 3;
}

void move_player(int player_idx,char c) //  not reassgined 100 power to dead player neither manipulated points
{
	CLIENT cur = sends.clients[player_idx];
	int idx  = map_char_to_idx(c);
	int cx = cur.x;
	int cy = cur.y;
	int nx = cur.x+dx[idx];
	int ny = cur.y+dy[idx];
	//int change_pos = sends.num_changes;

	if(sends.matrix[nx][ny].type == BLANK)
	{
		sends.matrix[nx][ny].type = player_idx+1;// check
		sends.matrix[nx][ny].direction = sends.matrix[cur.x][cur.y].direction;
		sends.matrix[cur.x][cur.y].type = BLANK;
		sends.matrix[cur.x][cur.y].direction  = -1;
		cur.x = nx;
		cur.y = ny;
	}// BLANK
	else if(sends.matrix[nx][ny].type == BULLETS)
	{
		cur.health = max(0,cur.health-20);
		if(cur.health > 0)// alive same as blank cell
		{
			//**DELETE BULLET OTHERWISE BULLET WILL PASS THROUGH THE PERSON
			sends.matrix[nx][ny].type = player_idx+1;
			sends.matrix[nx][ny].direction = sends.matrix[cur.x][cur.y].direction;
			sends.matrix[cur.x][cur.y].type = BLANK;
			sends.matrix[cur.x][cur.y].direction  = -1;
			cur.x = nx;
			cur.y = ny;
		}
		else // dead
		{
			//**DELETE BULLET OTHERWISE BULLET WILL PASS THROUGH THE PERSON
			sends.matrix[nx][ny].type = BLANK;
			sends.matrix[nx][ny].direction = -1;
			sends.matrix[cur.x][cur.y].type = BLANK;
			sends.matrix[cur.x][cur.y].direction  = -1;
			int nposx;int nposy;
			get_Pos(&nposx,&nposy);
			cur.x = nposx;
			cur.y = nposy;
			//**NEW PLAYER ASSIGN ALL THE VARIABLE AND ADD THIS PLAYER TO CHANGES
		}
	}// BULLET
	// fill 
	/*++change_pos;
	sends.changes[change_pos].row = cx;
	sends.changes[change_pos].col = cy;
	sends.changes[change_pos].cell = sends.matrix[cx][cy];
	++change_pos;
	sends.changes[change_pos].row = nx;
	sends.changes[change_pos].col = ny;
	sends.changes[change_pos].cell = sends.matrix[nx][ny];
	sends.num_changes = change_pos;
	// while leaving reassign cur's everything to CLIENT[player_idx] to reflect changes*/
	sends.clients[player_idx] = cur;
	global_changes++;
}

int delete_or_not(BULLET *bul) // reassgined 100 health to dead player but not increases points of killer
{
	printf("New bullet\n");
	int ret = 0;
	int dir = bul->dir;
	int nx = bul->x;
	int ny = bul->y;
	if(dir == 1)//u
		nx--;
	if(dir == 2)//d
		nx++;
	if(dir == 3)//r
		ny++;
	if(dir == 4)//l
		ny--;
	//int cpos = sends.num_changes;
	if(sends.matrix[nx][ny].type == BRICK)
	{
		printf("in brick %d %d %d %d\n",bul->x,bul->y,nx,ny);
		printf("%d\n",sends.matrix[nx][ny].type);

		sends.matrix[bul->x][bul->y].type = BLANK;
		sends.matrix[bul->x][bul->y].direction = -1;
		global_changes++;

		/*cpos++;
		sends.changes[cpos].row = bul->x;
		sends.changes[cpos].col = bul->y;
		sends.changes[cpos].cell = sends.matrix[bul->x][bul->y];*/
		ret = 1;
	}
	else if(sends.matrix[nx][ny].type == BLANK || sends.matrix[nx][ny].type == BULLETS)
	{
		printf("in bullets %d %d %d %d\n",bul->x,bul->y,nx,ny);
		printf("%d\n",sends.matrix[nx][ny].type);
		
		sends.matrix[nx][ny].type = BULLETS;
		sends.matrix[nx][ny].direction = sends.matrix[bul->x][bul->y].direction;
		sends.matrix[bul->x][bul->y].type = BLANK;
		sends.matrix[bul->x][bul->y].direction = -1;
		global_changes++;

		/*cpos++;
		sends.changes[cpos].row = bul->x;
		sends.changes[cpos].col = bul->y;
		sends.changes[cpos].cell = sends.matrix[bul->x][bul->y];
		cpos++;
		sends.changes[cpos].row = nx;
		sends.changes[cpos].col = ny;
		sends.changes[cpos].cell = sends.matrix[nx][ny];*/
		bul->x = nx;
		bul->y = ny;
		ret = 0;
	}
	else // player
	{
		printf("in player\n");
		int player_idx = sends.matrix[nx][ny].type-1;
		CLIENT cur = sends.clients[player_idx];
		if(cur.health > 20)// alive netx is same bullet goes off 
		{
			cur.health-=20;
		}
		else // he'll be 1.5
		{
			//**CREATING NEW PLAYER ASSIGN ALL VARIABLES ADD NEW PLAYER TO CHANGES
			int nposx,nposy;
			get_Pos(&nposx,&nposy);
			cur.x = nposx;
			cur.y = nposy;
			cur.health = 100;
			sends.matrix[nx][ny].type = BLANK;
			sends.matrix[nx][ny].direction = -1;
			next_spawn[player_idx] = sends.sqno-10;
			global_changes++;
			/*cpos++;
			sends.changes[cpos].row = nx;
			sends.changes[cpos].col = ny;
			sends.changes[cpos].cell = sends.matrix[nx][ny];*/
		}
		sends.matrix[bul->x][bul->y].type = BLANK;
		sends.matrix[bul->x][bul->y].direction = -1;
		global_changes++;
		/*cpos++;
		sends.changes[cpos].row = bul->x;
		sends.changes[cpos].col = bul->y;
		sends.changes[cpos].cell = sends.matrix[bul->x][bul->y];*/
		// reassign at player_idx,cur;
		sends.clients[player_idx] = cur;
		ret = 1;
	}
	//sends.num_changes = cpos;
	//printf("Num of changes %d\n",cpos);
	return ret;
}

void move_bullets() 
{
	//printf("in move_bullets\n");
	BULLET *trav = bullet;
	while(trav!=NULL)
	{
		if(delete_or_not(trav))
			trav->to_delete = 1;
		else
			trav->to_delete = 0;
		trav = trav->next;
	}
	del_bullet();
}

void fill(int maze, int start_row,int end_row, int start_col, int end_col){
	int i=maze;
	int j=start_row;
	int m=start_col;
	for(;j<=end_row;j++){
		for(m=start_col;m<end_col;m++){
			mazedata[i][j][m].type=	BRICK;
		}
	//printf("%d\n",j);
	}

}

void print(){
	int j,m;
	for(j=0;j<DIMENSION2;j++){
		for(m=0;m<DIMENSION1;m++){
			printf("%d",mazedata[0][j][m].type);
		}
		printf("\n");	
	}
}

void generate_maze(){
	int i,j,m;


	for(i=0;i<MAZES;i++){
		 for(j=0;j<DIMENSION2;j++){
			for(m=0;m<DIMENSION1;m++){
				mazedata[i][j][m].type=0;
				mazedata[i][j][m].direction=0;
			}	
		 }	
	}
	for(i=0; i<MAZES;i++){
		for(j=0;j<DIMENSION1;j++){
			mazedata[i][0][j].type=	BRICK;
			mazedata[i][DIMENSION2-1][j].type= BRICK; 
		}	
	}
	for(i=0; i<MAZES;i++){
		for(j=0;j<DIMENSION2;j++){
			mazedata[i][j][0].type=	BRICK;
			mazedata[i][j][DIMENSION1-1].type= BRICK;
	
		}	
	}
//first maze
	fill(0,4,8,0,10);
	fill(0,45,49,0,20);
	fill(0,20,24,10,30);
	fill(0,10,40,20,24);
	fill(0,4,8,35,45);
	fill(0,4,8,DIMENSION1-30,DIMENSION1);
	//fill(0,10,40,110,125);
	fill(0,25,30,DIMENSION1-20,DIMENSION1);
	//fill(0,72,76,40,44);
	//fill(0,70,72,40,55);
	//fill(0,70,72,70,80);
	//fill(0,41,44,55,80);
	fill(0,0,35,55,59);
	fill(0,16,20,59,65);
	fill(0,25,29,59,70);
}

void get_maze(){
	int j,m;
	for(j=0;j<DIMENSION2;j++){
		for(m=0;m<DIMENSION1;m++){
			sends.matrix[j][m].type=mazedata[0][j][m].type;
			sends.matrix[j][m].direction=mazedata[0][j][m].direction;
		}
	}

	int x,y;
	for(int i=0;i<=sends.num_players;i++)
	{	
		printf("reached here\n");
		get_Pos(&x,&y);
		printf("position of player %d %d",x,y);
		sends.matrix[x][y].type = i+1;
		sends.matrix[x][y].direction = DOWN;
		sends.clients[i].x = x;
		sends.clients[i].y = y;
		sends.clients[i].health = 100;
	}
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
	serverAddr.sin_addr.s_addr = inet_addr("172.17.46.242");
	bind(socketfd,(struct sockaddr *)&serverAddr,sizeof(serverAddr));
	addr_size = sizeof(serverAddr);

	memset(buffer,'\0',sizeof(buffer));
	//memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
	
	generate_maze();
	//get_maze();
	//print();
	while(1)
	{

		n = recvfrom(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,&addr_size);
		printf("%s\n",buffer);
		int t = check_player(clientAddr);
		printf("%d\n",t);
		if(t==-1)
		{
			if(sends.num_players==(MAX_PLAYERS-1))
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
				for(int i=0;i<=sends.num_players;i++)
				{
					printf("Name1 - %d\n",sends.num_players);
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
			for(int i=0;i<=sends.num_players;i++)
			{
				printf("Name2 - %d\n",sends.num_players);
				printf("Name2 - %s\n",(sends.clients[i]).name);
				sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
			}
		}

		if(t=all_ready())
		{
			printf("%d\n",t);
			strcpy(buffer,"$"); 
			for(int i=0;i<=sends.num_players;i++)
				sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
			break;
		}	
	}
	printf("out\n");

	get_maze();
	sends.sqno = 100;
	int prev = sends.sqno;

	while(!all_received())
	{	
		for(int i=0;i<=sends.num_players;i++)
		{
			printf("printing i %d\n",i);
			//sendto(socketfd,sends.matrix,sizeof(sends.matrix),0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
			//memset(se,'\0',sizeof(buffer));
			sends.msg[0] = i+'0';
			printf("%d\n",sends.msg[0]-'0');
			int n = sendto(socketfd,&sends,sizeof(SEND),0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
			printf("%d\n",n);
			if(n>0)
				printf("sending matrix---\n");
			memset(buffer,'\0',sizeof(buffer));
			recvfrom(socketfd,buffer,1024,MSG_DONTWAIT,(struct sockaddr*)&clientAddr,&addr_size);
			if(buffer[0]=='$')
			{
				printf("received ackn'\n");
				received[i] = 1;
			}
		}
		usleep(50000);
	}
	printf("done\n");
	while(1)
	{
		n = recvfrom(socketfd,buffer,1024,MSG_DONTWAIT,(struct sockaddr*)&clientAddr,&addr_size);
		
		if(n>0)
		{
			printf("here2\n");
			printf("%s\n",buffer);
			int t = check_player(clientAddr);
			if(t==-1)
			{
				printf("USER NOT A PART OF GAME\n");
				continue;
			}

			if(strlen(buffer)==2 && buffer[1]=='*')
			{
				if(buffer[0]==' ')
				{
					int x = (sends.clients[t]).x;
					int y = (sends.clients[t]).y;
					//keep direction sends.matrix[x][y].direction
					BULLET *temp = make_bullet(x+1,y+1,RIGHT,t);
					printf("in bullet %d %d %s\n",x+1,y+1,(sends.clients[t]).name);
					if(bullet==NULL)
						bullet = temp;
					else
					{
						temp->next = bullet;
						bullet = temp;
					}
				}
				else
					move_player(t,buffer[0]);
			}
			else
			{
				int n = strlen(sends.msg);
				sends.msg[n] = '*';
				n++;
				char str[100] = {"Invalid input by "};
				strcat(str,(sends.clients[t]).name);
				strcpy(sends.msg+n,str);
			}
			memset(buffer,'\0',sizeof(buffer));
		}

		move_bullets();
		sends.sqno--;
		printf("sequence nnumber %d\n",sends.sqno);
		respawn();
		//ADD A TIME LIMIT HERE
		usleep(70000);
		
		if(sends.sqno==(prev-100) || global_changes!=-1)
		{
			for(int i=0;i<=sends.num_players;i++)
			{
				sendto(socketfd,&sends,sizeof(SEND),0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
				//sendto(socketfd,sends.matrix,sizeof(sends.matrix),0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
			}
			memset(sends.msg,'\0',1024);	
			global_changes = -1;
			prev = sends.sqno;
		}

		//LEVEL FINISH
		if(sends.sqno==0)
		{
			for(int i=0;i<=sends.num_players;i++)
				received[i] = 0;
			while(!all_received())
			{	
				for(int i=0;i<=sends.num_players;i++)
				{	
					strcpy(sends.msg,"***");
					int n = sendto(socketfd,&sends,sizeof(SEND),0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
					//int n = sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
					//int n = sendto(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,addr_size);
					if(n>0)
						printf("sent----------\n");
					memset(buffer,'\0',sizeof(buffer));
					recvfrom(socketfd,buffer,1024,MSG_DONTWAIT,(struct sockaddr*)&clientAddr,&addr_size);
					if(buffer[0]=='$')
					{
						printf("received new ack\n");
						received[i] = 1;
					}
				}
			}
			printf("LEVEL ending\n");
			break;
		}
	}

	//NEXT LEVEL
	//making evveryone not ready
	for(int i=0;i<=sends.num_players;i++)
		(sends.clients[i]).flag = 2;
	//sleep(1);
	get_team_list();
	printf("team list %s\n",buffer);
	for(int i=0;i<=sends.num_players;i++)
		sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
		//sendto(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,addr_size);
	int t;	
	while(1)
	{	
		n = recvfrom(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,&addr_size);
		printf("%s\n",buffer);
		if(n>0)
		{
			printf("received\n");
			t = check_player(clientAddr);
			printf("%d\n",t);

			if(t==-1)
				printf("USER NOT A PART OF GAME\n");//valid input
			else if(buffer[0]=='*' && (buffer[1]=='1' || buffer[1]=='2' ))
			{
				sends.clients[t].flag = buffer[1]-'0';
				sends.clients[t].teamno = buffer[2]-'0';

				get_team_list();
				printf("team list %s\n",buffer);
				for(int i=0;i<=sends.num_players;i++)
					sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
					//sendto(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,addr_size);
			}
			else
				printf("in else\n");	
		}
		else
			printf("neagtive n\n");
		
		if(t=all_ready())
		{
			printf("%d\n",t);
			strcpy(buffer,"$"); 
			for(int i=0;i<=sends.num_players;i++)
				sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
			break;	
		}	
	}
	printf("LEVEL 2 STARTING\n");
	return 0;
}