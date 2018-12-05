#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/netfilter.h>
#include <errno.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include<string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h> 
#include<pthread.h>
#include<vector>

using namespace std;



pthread_mutex_t mutex;
vector<sockaddr_in> client_socket;
vector<int> client_id;
#define BUFSIZE 1<<11
char buf[BUFSIZE];
void *thr(void *arg){
    setvbuf(stdin,NULL,_IONBF,0);setvbuf(stdout,NULL,_IONBF,0);
    
    pthread_mutex_lock(&mutex);
    int sockfd=*((int *)arg);
    int place=-1;
    for(int x=0;x<client_id.size();x++){
        if(sockfd==client_id[x]){
            place=x;
            break;
        }
    }
    if(place<0){
        printf("ERROR : Can't find socket\n");
        close(sockfd);
        exit(-1);
    }
    
    char *host_addr=inet_ntoa(client_socket[place].sin_addr);
    if(host_addr==NULL){
        printf("ERROR : Can't inet_ntoa\n");
        close(sockfd);
        exit(-1);
    }

    struct hostent *host=gethostbyaddr(&client_socket[place].sin_addr.s_addr,sizeof(client_socket[place].sin_addr.s_addr),AF_INET);
    if(host==NULL){
        printf("ERROR : Can't gethostbyaddr\n");
        close(sockfd);
        exit(-1);
    }
    pthread_mutex_unlock(&mutex);
    printf("%d socket connected with %s\n",sockfd,host->h_name);

    while(true){
        memset(buf,0,sizeof(buf));
        if(read(sockfd,buf,sizeof(buf)==0))break;
        printf("Proxy : %d socket <- %s\n",sockfd,host->h_name);
        int buflen=strlen(buf);

        char host[1<<8];
        for(int x=0;x<buflen;x++){
            if(strncmp(buf+x,"Host: ",6)==0){
                int i=x+6;
                int j=0;
                while(!(buf[i]==0x0d&&buf[i+1]!=0x0a))  host[j++]=buf[i];
                host[j]=0;
                break;
            }
        }

        int sockfd2=socket(AF_INET,SOCK_STREAM,0);
        if(sockfd2<0){
            printf("ERROR : Can't open socket\n");
            close(sockfd);
            close(sockfd2);
            exit(-1);
        }

        struct hostent *server=gethostbyname(host);
        if(server==NULL){
            printf("ERROR : Can't find host(%s)\n",host);
            close(sockfd);
            close(sockfd2);
            exit(-1);
        }

        struct sockaddr_in server_addr;
        memset(&server_addr,0,sizeof(server_addr));
        server_addr.sin_family=AF_INET;
        server_addr.sin_port=htons(80);
        memcpy(&server_addr.sin_addr.s_addr,server->h_addr_list[0],server->h_length);

        if(connect(sockfd2,(const sockaddr *)&server_addr,sizeof(server_addr))<0){
            printf("ERROR : Can't connect\n");
            close(sockfd);
            close(sockfd2);
            exit(-1);
        }
        
        printf("Proxy : Connecting to %s...\n",host);

        if(write(sockfd2,buf,strlen(buf))<0){
            printf("ERROR : Can't write\n");
            close(sockfd);
            close(sockfd2);
            exit(-1);
        }

        read(sockfd,buf,sizeof(buf));

        if(write(sockfd,buf,strlen(buf))<0){
            printf("ERROR : Can't write\n");
            close(sockfd);
            close(sockfd2);
            exit(-1);
        }
        printf("Proxy : Reply(%d Socket)\n",sockfd);
        close(sockfd2);
    }

    pthread_mutex_lock(&mutex);
    
    client_id.erase(client_id.begin()+place);
    client_socket.erase(client_socket.begin()+place);

    pthread_mutex_unlock(&mutex);
    printf("%d socket disconnected with %s\n",sockfd,host->h_name);

    close(sockfd);
}

int main(int argc,char **argv){
    if(argc!=2){
        printf("syntax : http_proxy <port>\n");
        printf("sample : http_proxy 8080\n");
        exit(-1);
    }
    
    setvbuf(stdin,NULL,_IONBF,0);setvbuf(stdout,NULL,_IONBF,0);
    pthread_t thread;
    int port=atoi(argv[1]);
    int parentfd=socket(AF_INET,SOCK_STREAM,0);
    if(parentfd<0){
        printf("ERROR : Can't open socket\n");
        close(parentfd);
        exit(-1);
    }
    int optval=1;
    setsockopt(parentfd,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(int));
    
    struct sockaddr_in server;
    memset(&server,0,sizeof(server));
    server.sin_family=AF_INET;
    server.sin_port=htons(port);
    server.sin_addr.s_addr=htonl(INADDR_ANY);

    if(bind(parentfd,(const struct sockaddr *)&server,sizeof(server))<0){
        printf("ERROR : Can't bind\n");
        close(parentfd);
        exit(-1);
    }

    if(listen(parentfd,5)<0){
        printf("ERROR : Can't listen\n");
        close(parentfd);
        exit(-1);
    }

    pthread_mutex_init(&mutex,NULL);
    struct sockaddr_in client;
    int cl=sizeof(struct sockaddr_in);
    while(true){
        int childfd=accept(parentfd,(sockaddr *)&client,(socklen_t *)&cl);
        if(childfd<0){
            printf("ERROR : Can't accept\n");
            exit(-1);
        }
        pthread_mutex_lock(&mutex);
        client_socket.push_back(client);
        client_id.push_back(childfd);
        pthread_mutex_unlock(&mutex);

        int thread_id=pthread_create(&thread,NULL,thr,(void *)&childfd);
        if(thread_id){
            printf("ERROR : Can't create thread\n");
            exit(-1);
        }
        pthread_detach(thread);
    }
    close(parentfd);
    return 0;
}