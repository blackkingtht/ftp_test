/*************************************************************************
   > File Name: server.c
   > Author: blackking
   > Mail: 459889714@qq.com
   > Created Time: 2018年01月26日 星期五 11时04分08秒
 ************************************************************************/

#include <pthread.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <linux/socket.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>

static int nthreads;
pthread_mutex_t clifd_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t clifd_cond = PTHREAD_COND_INITIALIZER;

typedef struct{
	pthread_t thread_tid;
	long thread_count;
}Thread;  
Thread *tptr;

struct request{
	int loadflag;  //0 upload 1 download
	char filename[50];
	int offset;
};

#define MAXN 16384
#define MAXNCLI 32
#define MAXLINE 4096
#define LISTENQ 1024
int clifd[MAXNCLI], iget, iput;

void thread_make(int);
void *thread_main(void *);
void web_child(int);
int tcp_listen(const char* , const char *, socklen_t*);



int main(int argc, char *argv[])
{
	int i, listenfd, connfd;
	socklen_t addrlen, clilen;
	struct sockaddr *cliaddr;
	if(argc == 3){
		listenfd = tcp_listen(NULL, argv[1], &addrlen);
	}
	else if(argc == 4){
		listenfd = tcp_listen(argv[1], argv[2], &addrlen);
	}
	else {
		printf("参数错误\n");
	}
	cliaddr = malloc(addrlen);

	nthreads = atoi(argv[argc - 1]);
	tptr = calloc(nthreads, sizeof(Thread));
	iget = iput = 0;

	for(i = 0; i < nthreads; i++){
		thread_make(i);
	}

	for(; ;){
		clilen = addrlen;

		connfd = accept(listenfd, cliaddr, &clilen);
		pthread_mutex_lock(&clifd_mutex);
		clifd[iput] = connfd;
		if(++iput == MAXNCLI){
			iput = 0;
		}
		if(iput == iget){
			printf("iput = iget = %d\n", iput);
		}
		pthread_cond_signal(&clifd_cond);
		pthread_mutex_unlock(&clifd_mutex);
	}

}

void thread_make(int i){
	pthread_create(&tptr[i].thread_tid, NULL, &thread_main, (void *) i);
	return;
}

void *thread_main(void *arg){
	int connfd;

	printf("thread %d starting\n", (int) arg);

	for(; ;){
		pthread_mutex_lock(&clifd_mutex);
		while(iget == iput)
			pthread_cond_wait(&clifd_cond, &clifd_mutex);

		connfd = clifd[iget];
		if(++iget == MAXNCLI){
			iget = 0;
		}
		pthread_mutex_unlock(&clifd_mutex);

		tptr[(int)arg].thread_count++;
		web_child(connfd);
		close(connfd);
	}
}

void web_child(int sockfd){
	//printf("sockfd: %d\n", sockfd);
	struct request clirequest;
	ssize_t rc;
	char buf[MAXN];
	struct stat fileview;
	//struct sockaddr_in client_addr;
	char pathname[50];
	FILE *fp;

	/*getpeername(sockfd, (struct sockaddr*)&client_addr, sizeof(struct sockaddr));
	string dir_name = inet_ntoa(client_addr.sin_addr);
	sprintf(pathname, "./%s", dir_name);

	if(access(pathname, F_OK) == -1){
		mkdir(pathname, 755);  
	}*/  
	//是否为不同的主机创建文件夹

	recv(sockfd, &clirequest, sizeof(struct request), 0);
	sprintf(pathname, "./%s", clirequest.filename);
	if(clirequest.loadflag == 0){   //client upload to server
	  if(access(pathname, F_OK) == 0){  //文件存在
		stat(pathname, &fileview);
		ssize_t size = fileview.st_size;
		sprintf(buf, "%d", size);
		write(sockfd, buf, strlen(buf));

		memset(buf, 0, sizeof(buf));
		//printf("open %s ok!\n", pathname);
		fp = fopen(pathname, "ab+");
		while((rc = read(sockfd, buf, sizeof(buf))) != 0){
			fwrite(buf, sizeof(unsigned char), rc, fp);
			//fputs(buf, fp);
		}
		fclose(fp);
		return;
	  }
	  else{   //文件不存在
		
		write(sockfd, "0", sizeof(unsigned char));
		fp = fopen(pathname, "ab+");
		while((rc = read(sockfd, buf, sizeof(buf))) != 0){
			fwrite(buf, sizeof(unsigned char), rc, fp);
		}
		fclose(fp);
		return;
	  }
	}
	else{    //client download from server
	    if((fp = fopen(pathname, "rb")) == NULL){
		    strcpy(buf, "no this file!!");
			write(sockfd, buf, strlen(buf));
	    }
		else{
			strcpy(buf, "ready!!");
			write(sockfd, buf, strlen(buf));
			memset(buf, 0, sizeof(buf));
			fseek(fp, clirequest.offset, SEEK_SET);
			while((rc = fread(buf, sizeof(unsigned char), MAXN, fp)) != 0){
				write(sockfd, buf, rc);
				memset(buf, 0, sizeof(buf));
			}
			fclose(fp);
		}
		return;
	}
}

int tcp_listen(const char* host, const char *serv, socklen_t *addrlenq){
	int listenfd, n;
	const int on = 1;
	struct addrinfo hints, *res, *ressave;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	
	if((n = getaddrinfo(host, serv, &hints, &res)) != 0){   //hints填写期望返回的信息类型，返回的地址信息串填入res
		printf("tcp_listen error for %s, %s:%s\n", host, serv, gai_strerror(n));
	}
	ressave = res;
	do{
		listenfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

		if(listenfd < 0){
			continue;   //res是一个可能的地址信息串，以当前地址信息创建监听套接字失败，尝试下一个
		}

		setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));  //设置套接字选项，允许重用该地址
		if((n = bind(listenfd, res->ai_addr, res->ai_addrlen)) == 0){
			break;		//成功
		}
		close(listenfd);  //绑定失败
	}while((res = res->ai_next) != NULL);
	
	if(res == NULL){
		printf("tcp_listen error for %s, %s\n", host, serv);
	}

	listen(listenfd, LISTENQ);

	if(addrlenq){
		*addrlenq = res->ai_addrlen;
	}

	freeaddrinfo(ressave);
	return(listenfd);
}
