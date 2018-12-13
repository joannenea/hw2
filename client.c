#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <netdb.h>     
#include <pthread.h> 


#define PORT 8834   
#define MAXDATASIZE 1048576  //資料傳輸最多1MB
#define NONE "\033[m"
#define RED "\033[0;32;31m"
#define LIGHT_RED "\033[1;31m"
#define GREEN "\033[0;32;32m"
#define LIGHT_GREEN "\033[1;32m"
#define BLUE "\033[0;32;34m"
#define LIGHT_BLUE "\033[1;34m"
#define DARY_GRAY "\033[1;30m"
#define CYAN "\033[0;36m"
#define LIGHT_CYAN "\033[1;36m"
#define PURPLE "\033[0;35m"
#define LIGHT_PURPLE "\033[1;35m"
#define BROWN "\033[0;33m"
#define YELLOW "\033[1;33m"
#define LIGHT_GRAY "\033[0;37m"
#define WHITE "\033[1;37m"


char sendbuf[MAXDATASIZE];
char sendbuf2[MAXDATASIZE];
char recvbuf[MAXDATASIZE];
char buf[MAXDATASIZE];  
char name[100];
int recv_flag=0;
char filename[100];
int fd;

void sendfile(char* filename)
{   
    //開始傳送檔案
    fprintf(stderr, LIGHT_GREEN"開始傳送檔案\n"NONE);

    //取得檔案長度
    FILE *fp = fopen(filename, "r");
    fseek(fp, 0, SEEK_END);
	int file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
    
    char *file_data = malloc(file_size);
    memset(file_data, 0, sizeof(file_data));
    //讀取檔案
    fread(file_data, sizeof(char), file_size, fp);
    fclose(fp);
    
    //發送檔案資訊
    send(fd, file_data, file_size, 0);
    //完成檔案發送關閉連線
    return;
}

void pthread_recv(void* ptr)
{
    while(1)
    {
        
        if ((recv(fd,recvbuf,MAXDATASIZE,0)) == -1){ 
            printf(RED"recv() error\n"NONE); 
            exit(1); 
        } 
        if(recv_flag == 2){
            int filesize=0;
            int cnt =0;

            sscanf(recvbuf,"%d",&filesize);
            //fprintf(stderr,"%d",filesize);
            memset(recvbuf,0,MAXDATASIZE);
            int filenamelen = recv(fd,recvbuf,MAXDATASIZE,0);
            strncpy(filename,recvbuf,filenamelen);
            char path[1024];
            memset(path,0,1024);
            sprintf(path,"./download/%s",filename);
            printf(LIGHT_GREEN"recv file to %s\n"NONE,path);
            FILE *fp1 = fopen(path, "w");
            while(cnt<filesize)
            {
                memset(recvbuf,0,MAXDATASIZE);
                int nowsize = recv(fd,recvbuf,MAXDATASIZE,0);
                if (nowsize <= 0) break;
                fwrite(recvbuf, sizeof(char), nowsize, fp1);
                cnt+=nowsize;
                
            }
	        fclose(fp1);
            recv_flag = 0;
            printf(LIGHT_GREEN"下載完畢\n"NONE);
            continue;
        }
        printf("%s",recvbuf);
        
        char *p = strtok(recvbuf,":\r\n");
        if(p!=NULL){
            if(strcmp(p,"ready to accept file")==0)
            {   
                p = strtok(NULL,":\r\n");
                strcpy(filename,p);
                sendfile(filename);
            }
            else if(strcmp(p,"Please enter Y/N to recv file.")==0)
            {   
                recv_flag=1;
            }
        }
        
        memset(recvbuf,0,sizeof(recvbuf));
    }
}


int main(int argc, char *argv[]) 
{ 
    int  numbytes;    
    struct hostent *he;       
    struct sockaddr_in server; 
    int check=0; 

    if (argc !=2) {         
        printf(RED"Usage: %s <IP Address>\n"NONE,argv[0]); 
        exit(1); 
    } 

    if ((he=gethostbyname(argv[1]))==NULL){  
        printf(RED"gethostbyname() error\n"NONE); 
        exit(1); 
    } 

    if ((fd=socket(AF_INET, SOCK_STREAM, 0))==-1){ 
        printf(RED"socket() error\n"NONE); 
        exit(1); 
    } 

    bzero(&server,sizeof(server));

    server.sin_family = AF_INET; 
    server.sin_port = htons(PORT); 
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    if(connect(fd, (struct sockaddr *)&server,sizeof(struct sockaddr))==-1){  
        printf("connect() error\n"); 
        exit(1); 
    } 

    printf(LIGHT_GREEN"連線成功\n"NONE);
    printf(LIGHT_GREEN"請輸入您的用戶名："NONE);
    fgets(name,sizeof(name),stdin);
    name[strlen(name)-1] = '\0';
    send(fd,name,(strlen(name)),0);

    pthread_t tid;
    pthread_create(&tid,NULL,(void*)pthread_recv,NULL);
    
    while(1)
    {
        memset(sendbuf,0,sizeof(sendbuf));
        fgets(sendbuf,sizeof(sendbuf),stdin);

        //for recv file
        while (recv_flag == 1)
        {
            check=1;
            char *p = strtok(sendbuf, "\r\n");// Y or N?
            if (p == NULL) break;
            if (strcmp(p, "Y") == 0){
                recv_flag = 2;
                send(fd,"recv_yes",(strlen("recv_yes")),0);
                break;
            }
            else if (strcmp(p, "N") == 0){
                recv_flag = 0;
            }else{
                printf(LIGHT_GREEN"請輸入 Y/N : "NONE);
                fgets(sendbuf, MAXDATASIZE, stdin);
            }
        }
        if(check==1)
        {
            check=0;
            continue;
        }

        char *p = strtok(sendbuf, ",\r\n");
        if (p == NULL) continue;
        if(strcmp(p,"sendto") == 0){
            //送給特定對象
            p = strtok(NULL,",\r\n");
            if(p == NULL) continue;
            char to_user[100];
            strcpy(to_user,p);
            p = strtok(NULL,",\r\n");
            if(p == NULL) continue;
            memset(sendbuf2,0,sizeof(sendbuf2));
            sprintf(sendbuf2,"sendto,%s,%s,%s",to_user,p,name);
            send(fd, sendbuf2, strlen(sendbuf2), 0);
        }
        else if ((strcmp(p, "sendfileto") == 0)){
            //檔案發送指令
            p = strtok(NULL,",\r\n");
            if(p == NULL) continue;
            char to_user[100];
            strcpy(to_user,p);
            p = strtok(NULL,",\r\n");
            if(p == NULL) continue;
            memset(sendbuf2,0,sizeof(sendbuf2));
            sprintf(sendbuf2,"sendfileto,%s,%s,%s",to_user,p,name);
            send(fd, sendbuf2, strlen(sendbuf2), 0);
            
        }else if ((strcmp(p, "getuser") == 0)){
            //取得使用者列表
            send(fd, "getuser", strlen("getuser"), 0);
        }
        else if(strcmp(sendbuf,"exit")==0){
            memset(sendbuf,0,sizeof(sendbuf));
            printf(RED"您已退出群聊\n"NONE);
            send(fd,sendbuf,(strlen(sendbuf)),0);
            break;
        }
        else{
            //送給所有人
            memset(sendbuf2,0,sizeof(sendbuf2));
            sprintf(sendbuf2,"%s:%s\n",name,p);
            send(fd,sendbuf2,(strlen(sendbuf2)),0);
        }
    }
    close(fd);
    return 0;  
}
