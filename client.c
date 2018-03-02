/*************************************************************************
   > File Name: client.c
   > Author: blackking
   > Mail: 459889714@qq.com
   > Created Time: 2018年01月29日 星期一 15时24分25秒
 ************************************************************************/

#include <stdio.h> 
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/stat.h>

#define MAXN 16384
#define MAXLINE 4096

int tcp_connect(const char *, const char *);
struct request{
	int loadflag;   //0 upload 1 download
	char filename[50];
	int offset;
};

int main(int argc, char *argv[]){
	FILE *fp;
	int i, j, fd, offset, rc;
	pid_t pid;
	ssize_t n;
	char filename[MAXLINE], buf[MAXN];
	char *ptr;
	struct request clirequest,servrequest;
	struct stat fileview;


	if(argc != 5){   //.out host serv upload/download filename
		printf("parameter wrong!");
	}

	ptr = strrchr(argv[4], '/');
	
	sprintf(filename, "%s", ptr+1);   //从路径中提取文件名


	clirequest.loadflag = atoi(argv[3]);
	strcpy(clirequest.filename, filename);

	fd = tcp_connect(argv[1], argv[2]);
	if(clirequest.loadflag == 0){  //upload
		clirequest.offset = -1;
	}
	else if(clirequest.loadflag == 1){  //download
		if(access(argv[4], F_OK) == 0){
			stat(argv[4], &fileview);
			clirequest.offset = fileview.st_size;
		}
		else{
			clirequest.offset = 0;
		}
	}
	else{
		printf("parameter wrong 0 for upload, 1 for download\n");
		exit(1);
	}
	send(fd, &clirequest, sizeof(struct request), 0);

	if(clirequest.loadflag == 0){  //upload
		if((fp = fopen(argv[4], "rb")) == NULL){
			printf("open failed\n");
			exit(0);
		}

		read(fd, buf, sizeof(buf));
		offset = atoi(buf);
		printf("offset:%d\n", offset);
		fseek(fp, offset, SEEK_SET);
		memset(buf, 0, sizeof(buf));
		while((rc = fread(buf, sizeof(unsigned char), MAXN, fp)) != 0){
			printf("%s\n", buf);
			write(fd, buf, rc);
			memset(buf, 0, sizeof(buf));
		}
		fclose(fp);
		close(fd);
	}
	else{  //download
		read(fd, buf, sizeof(buf));
		if(strcmp(buf, "ready!!") == 0){
			if((fp = fopen(argv[4], "ab+")) == NULL){
				printf("open failed\n");
				exit(0);
			}
			memset(buf, 0, sizeof(buf));
			while((rc = read(fd, buf, sizeof(buf))) != 0){
				fwrite(buf, sizeof(unsigned char), rc, fp);
				memset(buf, 0, sizeof(buf));
			}
			fclose(fp);
		}
		else{
			printf("%s\n", buf);
		}
		close(fd);
	}
	//snprintf(request, sizeof(request), "%d\n", nbytes);

	/*for(i = 0; i < nchildren; i++){
		if((pid = fork()) == 0){
			for(j = 0; j < nloops; j++){
				fd = tcp_connect(argv[1], argv[2]);

				write(fd, request, strlen(request));

				if((n = read(fd, reply, nbytes)) != nbytes){
					printf("request counts wrong!\n");
				}

				printf("%s\n", reply);
				close(fd);
			}
			printf("child %d done!\n", i);
			exit(0);
		}
	}*/
	
	while(wait(NULL) > 0);

	if(errno != ECHILD){
		printf("wait error\n");
	}
	exit(0);
}


int tcp_connect(const char* host, const char* serv){
	int sockfd, n;
	struct addrinfo hints, *res, *resave;
	
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if((n = getaddrinfo(host, serv, &hints, &res)) != 0){
		printf("地址信息错误！\n");
	}

	resave = res;

	do{
		sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

		if(sockfd < 0){
			continue;
		}

		if(connect(sockfd, res->ai_addr, res->ai_addrlen) == 0){
			break;
		}
		close(sockfd);
	}while((res = res->ai_next) != NULL);

	if(res == NULL){
		printf("tcp_connect error\n");
	}

	freeaddrinfo(resave);

	return(sockfd);
}
