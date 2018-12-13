#include <stdio.h>          
#include <string.h>       
#include <unistd.h>          
#include <sys/types.h> 
#include <sys/stat.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <pthread.h> 
#include <stdlib.h>

#define PORT 8834    
#define BACKLOG 1 
#define Max 10 //最多10人連線
#define MAXSIZE 1048576 //資料傳輸最多1MB

/*global*/
int number = 0;
int fdt[Max]={0};
char mes[MAXSIZE];
char user_list[Max][100];
char str[1024];
char filename[1024];
int recv_flag = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//初始化使用者設定
int init_user(char *in, int user_fd)
{
	pthread_mutex_lock(&mutex);
	for(int i = 0; i < Max; i++){
		if(fdt[i] == user_fd){
			strcpy(user_list[i], in);
			printf("newuser:%s\n",user_list[i]);
			pthread_mutex_unlock(&mutex);
			return i;
		}
	}
	pthread_mutex_unlock(&mutex);
	return -1;
}

int send_msg(int fd,char* buf)
{
	for(int i=0;i<Max;i++){
		printf("fdt[%d]=%d\n",i,fdt[i]);
		if((fdt[i]!=0)&&(fdt[i]!=fd)){
			send(fdt[i],buf,strlen(buf),0); 
			printf("send message to %d\n",fdt[i]);
		}
	}   
	return 0;
}

int find_user(char *name)
{
	for(int i=0;i<Max;i++)
		if(strcmp(user_list[i],name)==0)
			return i;
	return -1;
}

int find_fd(int fd)
{
	for(int i=0;i<Max;i++)
		if(fdt[i]==fd)
			return i;
	return -1;
}

void sendfiletoclient(int fd,char *file)
{  
	recv_flag = 0;
	char path[1024];
	memset(path,0,1024);
	int target = find_fd(fd);
	sprintf(path,"./file/%s/%s",user_list[target],file);
	printf("read file from %s\n",path);
	//讀檔
	FILE *fp = fopen(path, "r");
	fseek(fp, 0, SEEK_END);
	int file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	char *file_data = malloc(file_size);
	memset(file_data,0,sizeof(file_data));
	sprintf(file_data,"%d",file_size);
	send(fd, file_data, strlen(file_data), 0);  
	sleep(1);

    memset(file_data,0,sizeof(file_data));
	sprintf(file_data,"%s",file);
	send(fd, file_data, strlen(file_data), 0);  
	sleep(1);

	memset(file_data, 0, sizeof(file_data));
	fread(file_data, sizeof(char), file_size, fp);
	fclose(fp);
	
	//發送檔案資訊
	send(fd, file_data, file_size, 0);
	//完成檔案發送關閉連線
	return;
}

void recvfile(int userfd,char *filename,int targetfd)
{
	memset(mes, 0, MAXSIZE);
	int numbytes=recv(userfd,mes,MAXSIZE,0);

	char path[1024];
	memset(path,0,1024);
	sprintf(path,"./file/%s",user_list[targetfd]);
	int status = mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH); 

	memset(path,0,1024);
	sprintf(path,"./file/%s/%s",user_list[targetfd],filename);

	FILE *fp1 = fopen(path, "w");
	fwrite(mes, sizeof(char), numbytes, fp1);
	fclose(fp1);

	//告知發送者發送到server成功
	memset(str, 0, sizeof(str));
	sprintf(str,"server recv file successful\n");
	send(userfd,str,sizeof(str),0);

	//告知接收端有檔案可以接收
	memset(str, 0, sizeof(str));
	sprintf(str,"Please enter Y/N to recv file.: %s\n",filename);
	send(fdt[targetfd],str,sizeof(str),0);
	return;
}

void *pthread_service(void* sfd)
{
	int fd=*(int *)sfd;
	int numbytes;
	int i;
	char str[1024];


	memset(mes, 0, MAXSIZE);
	numbytes=recv(fd,mes,MAXSIZE,0);
	if(numbytes<=0){
		printf("exit! fd=%d\n",fd);
		return NULL;
	}

	int user_id = init_user(mes, fd);
	if(user_id == -1){
		return NULL;
	}
	else{
		memset(str, 0, 1024);
		sprintf(str,"%s 進入聊天室\n",user_list[user_id]);
		send_msg(fd,str);
	}

	while(1)
	{
		memset(mes, 0, MAXSIZE);
		numbytes=recv(fd,mes,MAXSIZE,0);
		if(numbytes<=0){
			for(i=0;i<Max;i++)
				if(fd==fdt[i])
					fdt[i]=0;               

			printf("exit! fd=%d\n",fd);
			break;
		}

		printf("receive message from %d,size=%d\n",fd,numbytes);

		//檢查指令
		char ori[1024];
		strcpy(ori,mes);
		char *p = strtok(mes, ",\r\n");
		if (strcmp(p, "sendto") ==0){
			//發送訊息
			p = strtok(NULL, ",\r\n");

			int target=find_user(p);
			if(target==-1)
			{
				memset(str, 0, sizeof(str));
				strcpy(str,"該用戶不存在\n");
				send(fd,str,sizeof(str),0);
			}
			else{
				p = strtok(NULL, ",\r\n");
				if(p == NULL) continue;
				memset(str, 0, sizeof(str));
				sprintf(str,"用戶 %s 悄悄地對你說: %s\n",user_list[user_id],p);
				send(fdt[target],str,sizeof(str),0);
			}

		}
		else if (strcmp(p, "sendfileto")==0){
			//發送檔案
			p = strtok(NULL, ",\r\n");
			if(p == NULL) continue;

			int target=find_user(p);
			if(target==-1)
			{
				memset(str, 0, sizeof(str));
				strcpy(str,"該用戶不存在\n");
				send(fd,str,sizeof(str),0);
			}
			else{
				p = strtok(NULL, ",\r\n");
				if(p == NULL) continue;
				memset(str, 0, sizeof(str));
				sprintf(str,"ready to accept file:%s\n",p);
				send(fd,str,sizeof(str),0);

				//接收檔案到server
				memset(filename,0,1024);
				strcpy(filename,p);
				recvfile(fd,filename,target);
			}
		} 
		else if (strcmp(p, "getuser")==0){
			//取得使用者列表
			char user_name[Max*101];
			memset(user_name, 0, sizeof(user_name));
			for(int i = 0;i<Max;i++)
			{
				if (fdt[i] == 0) continue;
				strcat(user_name, user_list[i]);
				strcat(user_name, "\n");
			}
			send(fd,user_name,sizeof(user_name),0);
		}
		else if(strcmp(p,"recv_yes")==0)
		{
			//client確認接收檔案
			recv_flag = 1;
			sendfiletoclient(fd,filename);
		}
		else if (strcmp(p, "exit")==0){ 
			//離開聊天室
			pthread_mutex_lock(&mutex);
			fdt[user_id]=0;
			close(user_id);
			pthread_mutex_unlock(&mutex);
			return NULL;
		}
		else if (recv_flag!=1)
		{
			send_msg(fd,ori);
		}
	}
	close(fd);

}

int  main()  
{ 
	int listenfd, connectfd;    
	struct sockaddr_in server; 
	struct sockaddr_in client;      
	int sin_size = sizeof(struct sockaddr_in); 
	int fd;

	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
	{   
		perror("Creating socket failed.");
		exit(1);
	}

	int opt = SO_REUSEADDR;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	bzero(&server,sizeof(server));  
	server.sin_family=AF_INET; 
	server.sin_port=htons(PORT); 
	server.sin_addr.s_addr = htonl (INADDR_ANY); 

	if (bind(listenfd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1) { 
		perror("Bind error.");
		exit(1); 
	}   

	if(listen(listenfd,BACKLOG) == -1){  
		perror("listen() error\n"); 
		exit(1); 
	} 
	printf("等待連線中,請稍候\n");


	while(1)
	{
		if ((fd = accept(listenfd,(struct sockaddr *)&client,&sin_size))==-1) {
			perror("accept() error\n"); 
			exit(1); 
		}

		if(number>=Max){
			printf("no more client is allowed\n");
			close(fd);
		}

		int i;
		for(i=0;i<Max;i++){
			if(fdt[i]==0){
				fdt[i]=fd;
				break;
			}
		}

		pthread_t tid;
		pthread_create(&tid,NULL,(void*)pthread_service,&fd);
		number++;
	}
	close(listenfd);            
}
