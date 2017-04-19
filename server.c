#include <stdio.h>
#include <malloc.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

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
#define BRICK_WEAK 14
#define GRENADE 13
#define BLANK 0
#define DIMENSION 80
#define MAZES 10
#define DIMENSION1 110
#define DIMENSION2 40
#define EXITED 5
#define PAUSED 6
#define LEVEL_TIME 1000

int dx[5] = {0,-1,1,0,0};// left,up, right, down
int dy[5] = {0,0,0,1,-1};
int dirx[8] = {-1,-1,-1,0,1,1,1,0};
int diry[8] = {-1,0,1,1,1,0,-1,-1};
int next_spawn[12];
int gren[12];
int grenx[12];
int greny[12];
int global_changes = -1;
char global_str[200];
int brick_pow[DIMENSION2][DIMENSION1];
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

void append_msg(char str[200])
{
	int n = strlen(sends.msg);
	sends.msg[n] = '*';
	n++;
	sends.msg[n] = '\0';
	strcat(sends.msg,str);
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


void get_Pos(int *x,int *y)// dummy_function,modify this function to get a random position
{
	do
	{
		*x = rand()%DIMENSION2; 
		*y = rand()%DIMENSION1; 
	}while(sends.matrix[*x][*y].type!=BLANK);
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
	for (i = 0;i<=sends.num_players; ++i)
	{
		if(next_spawn[i] == sends.sqno && sends.clients[i].flag!=EXITED)
		{
			sends.matrix[sends.clients[i].x][sends.clients[i].y].type = i+1;
			sends.matrix[sends.clients[i].x][sends.clients[i].y].direction = DOWN;
			next_spawn[i] = -1; 
		}
	}
}
void blast()
{
	int i,j;
	for (i = 0;i<=sends.num_players; ++i)
	{
		if(gren[i] == sends.sqno && sends.clients[i].flag!=EXITED)
		{
			int posx = grenx[i];
			int posy = greny[i];
			for(j=0;j<8;j++)
			{
				if(posx+dirx[j] >= DIMENSION2-1 ||  posy+diry[j] >= DIMENSION1-1)
					continue;
				if(sends.matrix[posx+dirx[j]][posy+diry[j]].type>=1 && sends.matrix[posx+dirx[j]][posy+diry[j]].type<=sends.num_players+1)
				{
					if(sends.matrix[posx+dirx[j]][posy+diry[j]].type!=i+1) // not me 
					{
						sends.clients[i].points+=100;
					}
					int p_idx = sends.matrix[posx+dirx[j]][posy+diry[j]].type-1;
					next_spawn[p_idx] = sends.sqno-50;
					int nx,ny;
					get_Pos(&nx,&ny);
					sends.clients[p_idx].x = nx;
					sends.clients[p_idx].y = ny;
					sends.clients[p_idx].health = 100;
				}// kill
				if(sends.matrix[posx+dirx[j]][posy+diry[j]].type!=BLANK)
				{
					sends.matrix[posx+dirx[j]][posy+diry[j]].type = BLANK;
					sends.matrix[posx+dirx[j]][posy+diry[j]].direction = -1;
					++global_changes;
				}
			}
			gren[i] = grenx[i] = greny[i] = -1;
			sends.matrix[posx][posy].type = BLANK;
			sends.matrix[posx][posy].direction = -1;
			global_changes++;
		}
	}
}
void get_players_list()
{
	int ctr = 0;
	for(int i=0;i<=sends.num_players;i++)
	{
		if(sends.clients[i].flag==EXITED)
			continue;

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
		if(sends.clients[i].flag==EXITED)
			continue;
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
		if(sends.clients[i].flag==EXITED)
			continue;
		if((sends.clients[i]).flag==2)
			return 0;
	}
	return 1;
}

int all_received()
{
	for(int i=0;i<=sends.num_players;i++)
	{
		if(sends.clients[i].flag==EXITED)
			continue;
		if(received[i]==0)
			return 0;
	}
	return 1;
}

int map_char_to_idx(char c)
{
	if(c=='a')
		return LEFT;
	if(c=='w')
		return UP;
	if(c=='d')
		return RIGHT;
	if(c=='s')
		return DOWN;
}

void move_player(int player_idx,char c) //  not reassgined 100 power to dead player neither manipulated points
{
	CLIENT cur = sends.clients[player_idx];
	int idx  = map_char_to_idx(c);
	int cx = cur.x;
	int cy = cur.y;
	int nx = cur.x+dx[idx];
	int ny = cur.y+dy[idx];

	if(sends.matrix[cx][cy].direction!=idx)
	{
		sends.matrix[cx][cy].direction = idx;
		global_changes++;
	}
	else
	{
		if(sends.matrix[nx][ny].type == BLANK)
		{
			sends.matrix[nx][ny].type = player_idx+1;// check
			sends.matrix[nx][ny].direction = sends.matrix[cur.x][cur.y].direction;
			sends.matrix[cur.x][cur.y].type = BLANK;
			sends.matrix[cur.x][cur.y].direction  = -1;
			cur.x = nx;
			cur.y = ny;
			global_changes++;
		}// BLANK
		else if(sends.matrix[nx][ny].type == BULLETS)
		{
			/*cur.health = max(0,cur.health-20);
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
			}*/
		}// BULLET
		sends.clients[player_idx] = cur;
		//global_changes++;
	}
}

int delete_or_not(BULLET *bul) // reassgined 100 health to dead player but not increases points of killer
{
	printf("New bullet\n");
	
	int ret = 0;
	int dir = bul->dir;
	int nx = bul->x;
	int ny = bul->y;

	if(dir == UP)//u
		nx--;
	if(dir == DOWN)//d
		nx++;
	if(dir == RIGHT)//r
		ny++;
	if(dir == LEFT)//l
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
	else
	if(sends.matrix[nx][ny].type == BRICK_WEAK)
	{
		int c_health = brick_pow[nx][ny];
		c_health = max(0,c_health-20);
		brick_pow[nx][ny] = c_health;
		if(c_health == 0) // puts blank there
		{
			sends.matrix[nx][ny].type = BLANK;
			sends.matrix[nx][ny].direction = -1;
			global_changes++;
		}
		sends.matrix[bul->x][bul->y].type = BLANK;
		sends.matrix[bul->x][bul->y].direction = -1;
		global_changes++;
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
			if(cur.flag!=PAUSED)
				cur.health-=20;
		}
		else if(cur.flag!=PAUSED)// he'll be 1.5
		{
			//**CREATING NEW PLAYER ASSIGN ALL VARIABLES ADD NEW PLAYER TO CHANGES
			int killer = bul->p_num;

			if(sends.clients[killer].teamno==0 || sends.clients[killer].teamno!=cur.teamno)
			{
				sends.clients[killer].points += 100;
				int nposx,nposy;
				get_Pos(&nposx,&nposy);
				cur.x = nposx;
				cur.y = nposy;
				cur.health = 100;
				sends.matrix[nx][ny].type = BLANK;
				sends.matrix[nx][ny].direction = -1;
				next_spawn[player_idx] = sends.sqno-100;
				global_changes++;

				memset(global_str,'\0',sizeof(global_str));
				strcpy(global_str,sends.clients[killer].name);
				strcat(global_str," killed ");
				strcat(global_str,cur.name);
				append_msg(global_str);
			}
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

void fill(int maze,int type ,int start_row,int end_row, int start_col, int end_col){
	int i=maze;
	int j=start_row;
	int m=start_col;
	if(type==0){
	for(;j<=end_row;j++){
		for(m=start_col;m<end_col;m++){
			mazedata[i][j][m].type=	BRICK;
		}
	//printf("%d\n",j);
	}}
	else {
	for(;j<=end_row;j++){
		for(m=start_col;m<end_col;m++){
			mazedata[i][j][m].type=	BRICK_WEAK;
		}
	//printf("%d\n",j);
	}

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
	fill(0,0,4,8,0,10);
	//fill(0,45,49,0,20);
	fill(0,0,20,24,10,30);
	fill(0,0,10,40,20,24);
	fill(0,0,4,8,35,45);
	fill(0,0,4,8,DIMENSION1-30,DIMENSION1);
	//fill(0,10,40,110,125);
	fill(0,0,25,30,DIMENSION1-20,DIMENSION1);
	//fill(0,72,76,40,44);
	//fill(0,70,0,72,40,55);
	//fill(0,70,72,70,80);
	//fill(0,41,44,55,80);
	fill(0,0,0,35,55,59);
	fill(0,0,16,20,59,65);
	fill(0,0,25,29,59,70);
	fill(1,0,15,15,0,40);
	fill(1,0,10,15,40,42);
	fill(1,0,10,10,42,70);
	fill(1,0,10,15,70,72);
	fill(1,0,20,20,DIMENSION1-50,DIMENSION1);
	fill(1,0,20,25,DIMENSION1-52,DIMENSION1-50);
	fill(1,0,25,25,DIMENSION1-65,DIMENSION1-52);
	fill(1,0,25,30,DIMENSION1-67,DIMENSION1-65);
	fill(1,0,35,DIMENSION2,30,32);
	fill(1,0,35,35,32,DIMENSION1-30);
	fill(1,0,27,35,DIMENSION1-32,DIMENSION1-30);
	fill(2,0,4,8,0,10);
	//fill(3,45,49,0,20);
	fill(2,1,20,24,10,30);
	fill(2,0,10,19,20,24);
	fill(2,0,25,40,20,24);
	fill(2,0,4,8,35,45);
	fill(2,0,4,8,DIMENSION1-30,DIMENSION1);
	//fill(0,10,40,110,125);
	fill(2,0,25,30,DIMENSION1-20,DIMENSION1);
	//fill(0,72,76,40,44);
	//fill(0,70,72,40,55);
	//fill(0,70,72,70,80);
	//fill(0,41,44,55,80);
	fill(2,1,0,35,55,59);
	fill(2,1,16,20,59,65);
	fill(2,1,25,29,59,70);
}

void get_maze(int r){
	int j,m;
	for(j=0;j<DIMENSION2;j++){
		for(m=0;m<DIMENSION1;m++){
			sends.matrix[j][m].type=mazedata[r][j][m].type;
			sends.matrix[j][m].direction=mazedata[r][j][m].direction;
		}
	}

	int x,y;
	for(int i=0;i<=sends.num_players;i++)
	{
		if(sends.clients[i].flag==EXITED)
			continue;
		//if(sends.clients[i].flag==EXITED)
		//	continue;	
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

void fill_pow()
{
	int i,j;
	for(i=0;i<DIMENSION2;i++)
	{
		for (j = 0; j < DIMENSION1; ++j)
		{
			brick_pow[i][j] =  100;
		}
	}
}
int main()
{	
	srand(time(NULL));
	fill_pow();
	int socketfd,n;
	struct sockaddr_in serverAddr,clientAddr;
	socklen_t addr_size;

	socketfd = socket(PF_INET,SOCK_DGRAM,0);
	memset((char *)&serverAddr,0,sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(9000);
	serverAddr.sin_addr.s_addr = inet_addr("192.168.1.101");
	bind(socketfd,(struct sockaddr *)&serverAddr,sizeof(serverAddr));
	addr_size = sizeof(serverAddr);

	memset(buffer,'\0',sizeof(buffer));
	memset(next_spawn,-1,sizeof(next_spawn));
	memset(gren,-1,sizeof(gren));
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
					if(sends.clients[i].flag==EXITED)
						continue;
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
				if(sends.clients[i].flag==EXITED)
					continue;
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
			{
				if(sends.clients[i].flag==EXITED)
					continue;
				sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
			}
			break;
		}	
	}
	printf("out\n");

	get_maze(0);
	sends.sqno = LEVEL_TIME;
	int prev = sends.sqno;

	while(!all_received())
	{	
		for(int i=0;i<=sends.num_players;i++)
		{
			if(sends.clients[i].flag==EXITED)
				continue;
			printf("printing i %d\n",i);
			
			sends.msg[0] = i+'0';
			printf("%d\n",sends.msg[0]-'0');
			int n = sendto(socketfd,&sends,sizeof(SEND),0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
			printf("%d\n",n);
			if(n>0)
				printf("sending matrix---\n");//
			memset(buffer,'\0',sizeof(buffer));
			recvfrom(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,&addr_size);
		
			if(strlen(buffer)==2 && buffer[0]=='$')
			{
				printf("received ackn'\n");
				received[buffer[1]-'0'] = 1;
				printf("received ack from --> %d\n",buffer[1]-'0');
			}

			if(all_received())
				break;	
		}
		usleep(50000);
	}

	printf("done\n");
	while(1)
	{//SHIVAM GUPTA OF BITS PILA
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
			if(strlen(buffer)==2 && buffer[0]=='p' && buffer[1]=='*')
			{
				sends.clients[t].flag = PAUSED;
				sends.clients[t].points = max(0,sends.clients[t].points-20);
				
				memset(global_str,'\0',sizeof(global_str));
				strcpy(global_str,"Game Paused by ");
				strcat(global_str,(sends.clients[t]).name);
				append_msg(global_str);
			}
			else if(strlen(buffer)==2 && buffer[0]=='o' && buffer[1]=='*')
			{
				sends.clients[t].flag = EXITED;
				int curx = sends.clients[t].x;
				int cury = sends.clients[t].y;
				sends.matrix[curx][cury].type = BLANK;
				sends.matrix[curx][cury].direction = -1;
				global_changes++;

				memset(global_str,'\0',sizeof(global_str));
				strcpy(global_str,"Game Exited by ");
				strcat(global_str,(sends.clients[t]).name);
				append_msg(global_str);
			}
			else if(strlen(buffer)==2 && buffer[1]=='*')
			{
				if(buffer[0]==' ')
				{
					int x = (sends.clients[t]).x;
					int y = (sends.clients[t]).y;

					int nx = x + dx[sends.matrix[x][y].direction];
					int ny = y + dy[sends.matrix[x][y].direction];
					
					if(sends.matrix[nx][ny].type==BLANK)
					{
						BULLET *temp = make_bullet(nx,ny,sends.matrix[x][y].direction,t);
						printf("in bullet %d %d %s\n",nx,ny,(sends.clients[t]).name);
						if(bullet==NULL)
							bullet = temp;
						else
						{
							temp->next = bullet;
							bullet = temp;
						}
					}
				}
				else
				{
					sends.clients[t].flag = 1;
					move_player(t,buffer[0]);
				}
			}
			else
			{
				memset(global_str,'\0',sizeof(global_str));
				strcpy(global_str,"Invalid input by ");
				strcat(global_str,(sends.clients[t]).name);
				append_msg(global_str);
			}
			memset(buffer,'\0',sizeof(buffer));
		}

		move_bullets();
		sends.sqno--;
		//printf("sequence nnumber %d\n",sends.sqno);
		respawn();
		//blast();
		//ADD A TIME LIMIT HERE
		usleep(70000);
		
		if(sends.sqno==(prev-10) || global_changes!=-1)
		{
			printf("Message----- %s\n",sends.msg);
			for(int i=0;i<=sends.num_players;i++)
			{
				if(sends.clients[i].flag==EXITED)
					continue;
				sendto(socketfd,&sends,sizeof(SEND),0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
				//sendto(socketfd,sends.matrix,sizeof(sends.matrix),0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
			}
			//memset(sends.msg,'\0',1024);	
			global_changes = -1;
			prev = sends.sqno;

			if(sends.sqno%50==0)
				memset(sends.msg,'\0',sizeof(sends.msg));
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
					if(sends.clients[i].flag==EXITED)
						continue;
					//if(received[i]==1)
					//	continue;	
					strcpy(sends.msg,"***");
					int n = sendto(socketfd,&sends,sizeof(SEND),0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
					//int n = sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
					//int n = sendto(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,addr_size);
					if(n>0)
						printf("sent----------\n");
					memset(buffer,'\0',sizeof(buffer));
					recvfrom(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,&addr_size);
					
					if(strlen(buffer)==2 && buffer[0]=='$')
					{
						printf("received ackn'\n");
						received[buffer[1]-'0'] = 1;
						printf("received second ack from --> %s\n",sends.clients[buffer[1]-'0'].name);
					}
					if(all_received())
						break;	
				}
			}
			printf("LEVEL ending\n");
			break;
		}
	}

	strcpy(buffer,"$");
	for(int i=0;i<=sends.num_players;i++)
	{
		if(sends.clients[i].flag==EXITED)
			continue;
		sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
	}
	
	//NEXT LEVEL
	//making evveryone not ready
	for(int i=0;i<=sends.num_players;i++)
	{	
		if(sends.clients[i].flag==EXITED)
			continue;
		(sends.clients[i]).flag = 2;
	}
	//sleep(1);
	get_team_list();
	printf("team list %s\n",buffer);
	for(int i=0;i<=sends.num_players;i++)
	{
		if(sends.clients[i].flag==EXITED)
			continue;
		sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
	}	//sendto(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,addr_size);
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
				{
					if(sends.clients[i].flag==EXITED)
						continue;
					sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
				}	//sendto(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,addr_size);
			}
			else
				printf("in else\n");	
		}
		else
			printf("neagtive n\n");
		
		if(t=all_ready())
		{
			printf("All ready\n");
			printf("%d\n",t);
			strcpy(buffer,"$"); 
			for(int i=0;i<=sends.num_players;i++)
			{
				if(sends.clients[i].flag==EXITED)
					continue;
				sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
			}
			break;	
		}	
	}
	printf("LEVEL 2 STARTING\n");

	for(int i=0;i<=sends.num_players;i++)
	{
		if(sends.clients[i].flag==EXITED)
			continue;
		received[i] = 0;
	}
	memset(sends.msg,'\0',sizeof(sends.msg));

	//generate_maze();

	get_maze(1);
	fill_pow();
	printf("LEVEL 2 started\n");
	sends.sqno = LEVEL_TIME;
	prev = sends.sqno;
	memset(sends.msg,'\0',sizeof(sends.msg));

	//removing all bullets
	BULLET *trav = bullet;
	while(trav!=NULL)
	{
		trav->to_delete = 1;
		trav = trav->next;
	}
	del_bullet();

	//no spawning
	memset(next_spawn,-1,sizeof(next_spawn));
	memset(gren,-1,sizeof(gren));

	while(!all_received())
	{	
		for(int i=0;i<=sends.num_players;i++)
		{
			if(sends.clients[i].flag==EXITED)
				continue;
			printf("printing i %d\n",i);
			
			sends.msg[0] = i+'0';
			printf("%d\n",sends.msg[0]-'0');
			int n = sendto(socketfd,&sends,sizeof(SEND),0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
			printf("%d\n",n);
			if(n>0)
				printf("sending matrix2---\n");//
			memset(buffer,'\0',sizeof(buffer));
			recvfrom(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,&addr_size);
		
			if(strlen(buffer)==2 && buffer[0]=='$')
			{
				printf("received ackn'\n");
				received[buffer[1]-'0'] = 1;
				printf("received ack from --> %d\n",buffer[1]-'0');
			}

			if(all_received())
				break;	
		}
		usleep(50000);
	}

	///////////////////////////////////////////////////
	
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
			
			if(strlen(buffer)==2 && buffer[0]=='g' && buffer[1]=='*')
			{
				int dir = sends.matrix[sends.clients[t].x][sends.clients[t].y].direction;
				int nx = sends.clients[t].x;
				int ny = sends.clients[t].y;
				if(dir == UP)//u
					nx--;
				if(dir == DOWN)//d
					nx++;
				if(dir == RIGHT)//r
					ny++;
				if(dir == LEFT)//l
					ny--;
				if(sends.matrix[nx][ny].type == BLANK && gren[t] == -1)//  not already planted a bomb
				{
					grenx[t] = nx;
					greny[t] = ny;
					gren[t] = sends.sqno-50;
					sends.matrix[nx][ny].type = GRENADE;
					sends.matrix[nx][ny].direction = -1;
					global_changes++;
					
					memset(global_str,'\0',sizeof(global_str));
					strcpy(global_str,"Grenade placed by ");
					strcat(global_str,(sends.clients[t]).name);
					append_msg(global_str);
				}

				memset(global_str,'\0',sizeof(global_str));
				strcpy(global_str,"Grenade placed by ");
				strcat(global_str,(sends.clients[t]).name);
				append_msg(global_str);
			}
			else if(strlen(buffer)==2 && buffer[0]=='p' && buffer[1]=='*')
			{
				sends.clients[t].flag = PAUSED;
				sends.clients[t].points = max(0,sends.clients[t].points-20);
				
				memset(global_str,'\0',sizeof(global_str));
				strcpy(global_str,"Game Paused by ");
				strcat(global_str,(sends.clients[t]).name);
				append_msg(global_str);
			}
			else if(strlen(buffer)==2 && buffer[0]=='o' && buffer[1]=='*')
			{
				sends.clients[t].flag = EXITED;
				int curx = sends.clients[t].x;
				int cury = sends.clients[t].y;
				sends.matrix[curx][cury].type = BLANK;
				sends.matrix[curx][cury].direction = -1;
				global_changes++;

				memset(global_str,'\0',sizeof(global_str));
				strcpy(global_str,"Game Exited by ");
				strcat(global_str,(sends.clients[t]).name);
				append_msg(global_str);
			}
			else if(strlen(buffer)==2 && buffer[1]=='*')
			{
				if(buffer[0]==' ')
				{
					int x = (sends.clients[t]).x;
					int y = (sends.clients[t]).y;

					int nx = x + dx[sends.matrix[x][y].direction];
					int ny = y + dy[sends.matrix[x][y].direction];
					
					if(sends.matrix[nx][ny].type==BLANK)
					{
						BULLET *temp = make_bullet(nx,ny,sends.matrix[x][y].direction,t);
						printf("in bullet %d %d %s\n",nx,ny,(sends.clients[t]).name);
						if(bullet==NULL)
							bullet = temp;
						else
						{
							temp->next = bullet;
							bullet = temp;
						}
					}
				}
				else
				{
					sends.clients[t].flag = 1;
					move_player(t,buffer[0]);
				}
			}
			else
			{
				memset(global_str,'\0',sizeof(global_str));
				strcpy(global_str,"Invalid input by ");
				strcat(global_str,(sends.clients[t]).name);
				append_msg(global_str);
			}
			memset(buffer,'\0',sizeof(buffer));
		}

		move_bullets();
		sends.sqno--;
		respawn();
		blast();
		//ADD A TIME LIMIT HERE
		usleep(70000);
		
		if(sends.sqno==(prev-10) || global_changes!=-1)
		{
			printf("Message----- %s\n",sends.msg);
			for(int i=0;i<=sends.num_players;i++)
			{
				if(sends.clients[i].flag==EXITED)
					continue;
				sendto(socketfd,&sends,sizeof(SEND),0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
				//sendto(socketfd,sends.matrix,sizeof(sends.matrix),0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
			}
			global_changes = -1;
			prev = sends.sqno;

			if(sends.sqno%50==0)
				memset(sends.msg,'\0',sizeof(sends.msg));
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
					if(sends.clients[i].flag==EXITED)
						continue;
					//if(received[i]==1)
					//	continue;	
					strcpy(sends.msg,"***");
					int n = sendto(socketfd,&sends,sizeof(SEND),0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
					//int n = sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
					//int n = sendto(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,addr_size);
					if(n>0)
						printf("sent----------\n");
					memset(buffer,'\0',sizeof(buffer));
					recvfrom(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,&addr_size);
					
					if(strlen(buffer)==2 && buffer[0]=='$')
					{
						printf("received ackn'\n");
						received[buffer[1]-'0'] = 1;
						printf("received second ack from --> %s\n",sends.clients[buffer[1]-'0'].name);
					}
					if(all_received())
						break;	
				}
			}
			printf("LEVEL ending\n");
			break;
		}
	}



	//LEVEL 3
	strcpy(buffer,"$");
	for(int i=0;i<=sends.num_players;i++)
	{
		if(sends.clients[i].flag==EXITED)
			continue;
		sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
	}
	
	//NEXT LEVEL
	//making evveryone not ready
	for(int i=0;i<=sends.num_players;i++)
	{	
		if(sends.clients[i].flag==EXITED)
			continue;
		(sends.clients[i]).flag = 2;
		sends.clients[i].teamno = 0;
	}
	//sleep(1);
	get_team_list();
	printf("team list %s\n",buffer);
	for(int i=0;i<=sends.num_players;i++)
	{
		if(sends.clients[i].flag==EXITED)
			continue;
		sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
	}	//sendto(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,addr_size);
		
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
				{
					if(sends.clients[i].flag==EXITED)
						continue;
					sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
				}	//sendto(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,addr_size);
			}
			else
				printf("in else\n");	
		}
		else
			printf("neagtive n\n");
		
		if(t=all_ready())
		{
			printf("All ready\n");
			printf("%d\n",t);
			strcpy(buffer,"$"); 
			for(int i=0;i<=sends.num_players;i++)
			{
				if(sends.clients[i].flag==EXITED)
					continue;
				sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
			}
			break;	
		}	
	}

	printf("LEVEL 3 STARTING\n");

	for(int i=0;i<=sends.num_players;i++)
	{
		if(sends.clients[i].flag==EXITED)
			continue;
		received[i] = 0;
	}
	memset(sends.msg,'\0',sizeof(sends.msg));

	//generate_maze();
	get_maze(2);
	fill_pow();
	printf("LEVEL 3 started\n");
	sends.sqno = LEVEL_TIME;
	prev = sends.sqno;
	memset(sends.msg,'\0',sizeof(sends.msg));

	//removing all bullets
	trav = bullet;
	while(trav!=NULL)
	{
		trav->to_delete = 1;
		trav = trav->next;
	}
	del_bullet();

	//no spawning
	memset(next_spawn,-1,sizeof(next_spawn));
	memset(gren,-1,sizeof(gren));

	while(!all_received())
	{	
		for(int i=0;i<=sends.num_players;i++)
		{
			if(sends.clients[i].flag==EXITED)
				continue;
			printf("printing i %d\n",i);
			
			sends.msg[0] = i+'0';
			printf("%d\n",sends.msg[0]-'0');
			int n = sendto(socketfd,&sends,sizeof(SEND),0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
			printf("%d\n",n);
			if(n>0)
				printf("sending matrix3---\n");//
			memset(buffer,'\0',sizeof(buffer));
			recvfrom(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,&addr_size);
		
			if(strlen(buffer)==2 && buffer[0]=='$')
			{
				printf("received ackn'\n");
				received[buffer[1]-'0'] = 1;
				printf("received ack from --> %d\n",buffer[1]-'0');
			}

			if(all_received())
				break;	
		}
		usleep(50000);
	}

	///////////////////////////////////////////////////
	
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
			
			if(strlen(buffer)==2 && buffer[0]=='g' && buffer[1]=='*')
			{
				int dir = sends.matrix[sends.clients[t].x][sends.clients[t].y].direction;
				int nx = sends.clients[t].x;
				int ny = sends.clients[t].y;
				if(dir == UP)//u
					nx--;
				if(dir == DOWN)//d
					nx++;
				if(dir == RIGHT)//r
					ny++;
				if(dir == LEFT)//l
					ny--;
				if(sends.matrix[nx][ny].type == BLANK && gren[t] == -1)//  not already planted a bomb
				{
					grenx[t] = nx;
					greny[t] = ny;
					gren[t] = sends.sqno-50;
					sends.matrix[nx][ny].type = GRENADE;
					sends.matrix[nx][ny].direction = -1;
					global_changes++;
					
					memset(global_str,'\0',sizeof(global_str));
					strcpy(global_str,"Grenade placed by ");
					strcat(global_str,(sends.clients[t]).name);
					append_msg(global_str);
				}

				memset(global_str,'\0',sizeof(global_str));
				strcpy(global_str,"Grenade placed by ");
				strcat(global_str,(sends.clients[t]).name);
				append_msg(global_str);
			}
			else if(strlen(buffer)==2 && buffer[0]=='p' && buffer[1]=='*')
			{
				sends.clients[t].flag = PAUSED;
				sends.clients[t].points = max(0,sends.clients[t].points-20);
				
				memset(global_str,'\0',sizeof(global_str));
				strcpy(global_str,"Game Paused by ");
				strcat(global_str,(sends.clients[t]).name);
				append_msg(global_str);
			}
			else if(strlen(buffer)==2 && buffer[0]=='o' && buffer[1]=='*')
			{
				sends.clients[t].flag = EXITED;
				int curx = sends.clients[t].x;
				int cury = sends.clients[t].y;
				sends.matrix[curx][cury].type = BLANK;
				sends.matrix[curx][cury].direction = -1;
				global_changes++;

				memset(global_str,'\0',sizeof(global_str));
				strcpy(global_str,"Game Exited by ");
				strcat(global_str,(sends.clients[t]).name);
				append_msg(global_str);
			}
			else if(strlen(buffer)==2 && buffer[1]=='*')
			{
				if(buffer[0]==' ')
				{
					int x = (sends.clients[t]).x;
					int y = (sends.clients[t]).y;

					int nx = x + dx[sends.matrix[x][y].direction];
					int ny = y + dy[sends.matrix[x][y].direction];
					
					if(sends.matrix[nx][ny].type==BLANK)
					{
						BULLET *temp = make_bullet(nx,ny,sends.matrix[x][y].direction,t);
						printf("in bullet %d %d %s\n",nx,ny,(sends.clients[t]).name);
						if(bullet==NULL)
							bullet = temp;
						else
						{
							temp->next = bullet;
							bullet = temp;
						}
					}
				}
				else
				{
					sends.clients[t].flag = 1;
					move_player(t,buffer[0]);
				}
			}
			else
			{
				memset(global_str,'\0',sizeof(global_str));
				strcpy(global_str,"Invalid input by ");
				strcat(global_str,(sends.clients[t]).name);
				append_msg(global_str);
			}
			memset(buffer,'\0',sizeof(buffer));
		}

		move_bullets();
		sends.sqno--;
		respawn();
		blast();
		//ADD A TIME LIMIT HERE
		usleep(70000);
		
		if(sends.sqno==(prev-10) || global_changes!=-1)
		{
			printf("Message----- %s\n",sends.msg);
			for(int i=0;i<=sends.num_players;i++)
			{
				if(sends.clients[i].flag==EXITED)
					continue;
				sendto(socketfd,&sends,sizeof(SEND),0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
				//sendto(socketfd,sends.matrix,sizeof(sends.matrix),0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
			}
			global_changes = -1;
			prev = sends.sqno;

			if(sends.sqno%50==0)
				memset(sends.msg,'\0',sizeof(sends.msg));
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
					if(sends.clients[i].flag==EXITED)
						continue;
					//if(received[i]==1)
					//	continue;	
					strcpy(sends.msg,"***");
					int n = sendto(socketfd,&sends,sizeof(SEND),0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
					//int n = sendto(socketfd,buffer,1024,0,(struct sockaddr*)&((sends.clients[i]).address),addr_size);
					//int n = sendto(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,addr_size);
					if(n>0)
						printf("sent----------\n");
					memset(buffer,'\0',sizeof(buffer));
					recvfrom(socketfd,buffer,1024,0,(struct sockaddr*)&clientAddr,&addr_size);
					
					if(strlen(buffer)==2 && buffer[0]=='$')
					{
						printf("received ackn'\n");
						received[buffer[1]-'0'] = 1;
						printf("received second ack from --> %s\n",sends.clients[buffer[1]-'0'].name);
					}
					if(all_received())
						break;	
				}
			}
			printf("LEVEL ending\n");
			break;
		}
	}
	// call fill_pow when making maze for level 3
	return 0;
}
