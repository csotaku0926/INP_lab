#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <poll.h>
#include "read_write.h"
#include <signal.h>

// deal with broken pipe
void sigpipe_handler(int unused)
{
}

// date + message
void display_msg(int fd, char msg[]){
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char* format_msg = (char*)malloc(strlen(msg) * sizeof(char));
    snprintf(format_msg, strlen(msg), msg, 
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    Writen(fd, format_msg, strlen(msg));
}

// date + message
void display_msg1(int fd, char msg[], char arg1[]){
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char new_arg1[256];
    for(int i=0;i<256;i++){
        if(arg1[i]=='\n'){
            new_arg1[i] = 0;
            break;
        }else{
            new_arg1[i] = arg1[i];
        }
    }/*
    char* arg1_ptr;
    strcpy(arg1_ptr, new_arg1);*/
    char* format_msg = (char*)malloc((strlen(msg)+strlen(arg1)) * sizeof(char));
    snprintf(format_msg, strlen(msg)+strlen(new_arg1), msg, 
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, new_arg1);
    Writen(fd, format_msg, strlen(msg)+strlen(new_arg1));
}

// date + total user online
void display_msg2(int fd, char msg[], char arg1[], char arg2[]){
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char* format_msg = (char*)malloc(strlen(msg)+strlen(arg1)+strlen(arg2) * sizeof(char));
    snprintf(format_msg, strlen(msg)+strlen(arg1)+strlen(arg2), msg, 
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, arg1, arg2);
    Writen(fd, format_msg, strlen(msg)+strlen(arg1)+strlen(arg2));
}


int
main(int argc, char **argv)
{
	int					listenfd, connfd, infd;
	pid_t				childpid;
	socklen_t			clilen;
	struct sockaddr_in	cliaddr, servaddr;
	char				buffer[1024];

	if(argc != 2){
		printf("usage: <port>\n");
		return -1;
	}

    // deal with broken pipe
    signal(SIGPIPE, sigpipe_handler);

	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if(listenfd<0){
		perror("listen error: ");
		return -1;
	}


	// set to 0
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	//inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	int port = atoi(argv[1]);
	servaddr.sin_port        = htons(port);

	if((bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)))<0 ){
        perror("bind error: ");
		return -1;
    }

	if((listen(listenfd, 1024)) < 0){
        perror("listen error: ");
		return -1;
    }

    // poll
    int OPEN_MAX = 1100;
    struct pollfd client[OPEN_MAX]; // fds
    char recvline[128];
    client[0].fd = listenfd;
    client[0].events = POLLRDNORM;

    // other datas
    int total_user = 0;
    char* cliaddrs[OPEN_MAX];
    int cliports[OPEN_MAX];
    char* cli_names[OPEN_MAX];
    for (int i = 1; i < OPEN_MAX; i++)
        client[i].fd = -1; /* -1: available entry */
    int maxi = 0;
    int nready;

    //sigaction(SIGPIPE, &(struct sigaction){sigpipe_handler}, NULL);
    // main loop
    for(;;){
        nready = poll(client, maxi+1, -1);
        if(nready<0){
            perror("poll error: ");
            return -1;
        }

        if (client[0].revents & POLLRDNORM) { /* new client */
            clilen = sizeof(cliaddr);

            if ( (connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen)) < 0) {
                if (errno == EINTR)
                    continue;		/* back to for() */
                else
                    perror("accept error");
            }
            int i;
            for (i = 1; i < OPEN_MAX; i++)
                if (client[i].fd < 0) {
                    client[i].fd = connfd; /* save descriptor */
                    break;
                }

            if (i == OPEN_MAX){
                printf("too many clients");
                exit(-1);
            }
            client[i].events = POLLRDNORM;
            if (i > maxi) maxi = i; /* max index in client[] */
            
            // welcome
            display_msg(client[i].fd, "%d-%02d-%02d %02d:%02d:%02d *** Welcome to the simple CHAT server\n");
            printf("%s:%d just connected!\n",inet_ntoa(cliaddr.sin_addr),ntohs(cliaddr.sin_port));
            cliaddrs[i] = inet_ntoa(cliaddr.sin_addr);
            cliports[i] = ntohs(cliaddr.sin_port);
            total_user += 1;
            
            //
            // default user name
            char* user = (char*)malloc(10 * sizeof(char));
            snprintf(user, 10 ,"user %d",i);
            char* total_user_str = (char*)malloc(5 * sizeof(char));
            snprintf(total_user_str, 5, "%d", total_user);
            // online people msg
            display_msg2(client[i].fd, "%d-%02d-%02d %02d:%02d:%02d *** Total %s users now. Your name is <%s>\n"
                                , total_user_str, user);
            cli_names[i] = user;
            // someone just connected
            for(int j=1; j<=maxi; j++){
                if(i!=j){
                    display_msg1(client[j].fd, "%d-%02d-%02d %02d:%02d:%02d *** User <%s> just landed on server\n",
                                    cli_names[i]);
                }
            }
            if(--nready <= 0) continue; // no more readyable case
        }
        
        // 1000 client has bug below!
        int i;
        for (i = 1; i <= maxi; i++) {/* check all clients for data */
            int sockfd;
            if ( (sockfd = client[i].fd) < 0) continue;
            int n;
            if (client[i].revents & (POLLRDNORM | POLLERR)) {
                memset(recvline, 0, sizeof(recvline));
                if ( (n = read(sockfd, recvline, 128)) < 0) {
                    if (errno == ECONNRESET) {
                        /* connection reset by client */
                        printf("%s:%d reset\n",inet_ntoa(cliaddr.sin_addr),ntohs(cliaddr.sin_port));
                        close(sockfd);
                        client[i].fd = -1;
                        total_user -= 1;
                    } else{
                        perror("read error: ");
                        return -1;
                    }
                } else if (n == 0) { /* connection closed by client */
                    printf("%s:%d disconnect\n",cliaddrs[i],cliports[i]);
                    for(int j=1; j<=maxi; j++){
                        if(i!=j){
                            display_msg1(client[j].fd, "%d-%02d-%02d %02d:%02d:%02d *** User <%s> just left the server\n",
                                            cli_names[i]);
                        }
                    }
                    close(sockfd);
                    client[i].fd = -1;
                    total_user -= 1;
                } else{ //get something from cli!
                    // command
                    if(recvline[0]=='/'){
                        char* r2 = (char*)malloc(128 * sizeof(char));;
                        strcpy(r2, recvline);
                        // copied recvline
                        char* p = strtok(r2, " "); //split " "
                        if(strcmp(p, "/name")==0){
                            p = strtok(NULL, "\n");
                            char* real_name = p;
                            // check for name validity
                            if(strlen(real_name)==0){
                                // not valid cmd
                                //char* tmp = strtok(recvline, "\n"); //split " "
                                //recvline[strlen(recvline)-1] = 0;
                                display_msg1(client[i].fd, "%d-%02d-%02d %02d:%02d:%02d *** Unknown or incomplete command <%s>\n", recvline);
                                continue;
                            }
                            int name_valid = 0;
                            for(int k=0;k<strlen(real_name);k++){
                                if(real_name[k]!=' '){
                                    name_valid = 1;
                                    break;
                                }
                            }
                            if(name_valid==0){
                                display_msg(client[i].fd, "%d-%02d-%02d %02d:%02d:%02d *** No nickname given\n");
                                continue;
                            }
                            // display msg
                            for(int j=1;j<=maxi;j++){
                                if(i==j){
                                    display_msg1(client[j].fd, "%d-%02d-%02d %02d:%02d:%02d *** Nickname changed to <%s>\n",
                                                    real_name);
                                }
                                else{
                                    display_msg2(client[j].fd, "%d-%02d-%02d %02d:%02d:%02d *** User <%s> renamed to <%s>\n",
                                                    cli_names[i], real_name);
                                }
                            }
                            strcpy(cli_names[i], real_name);
                        }
                        // who command
                        else{
                            //char* p2 = strtok(recvline, "\n"); //split " "
                            if(strcmp(recvline, "/who\n")==0){
                                char* tmp = "--------------------------------------------------\n";
                                Writen(client[i].fd, tmp, strlen(tmp));
                                for(int j=1;j<=maxi;j++){
                                    if(client[j].fd>=0){
                                        // user name
                                        char user_name[20];
                                        user_name[0] = (i==j)?'*':' ';
                                        for(int k=1;k<20;k++){
                                            if(k-1<strlen(cli_names[j]))
                                                user_name[k] = cli_names[j][k-1];
                                            else
                                                user_name[k] = ' ';
                                        }

                                        //Writen(client[i].fd, cli_names[j], strlen(cli_names[j]));
                                        Writen(client[i].fd, user_name, 20);
                                        // cliaddr
                                        Writen(client[i].fd, cliaddrs[j], strlen(cliaddrs[j]));
                                        Writen(client[i].fd, ":", 1);
                                        char* port = (char*)malloc(6 * sizeof(char))  ;
                                        snprintf(port, 6, "%d",cliports[j]);
                                        Writen(client[i].fd, port, strlen(port));
                                        Writen(client[i].fd, "\n", 1);
                                    }
                                }
                                char* tmp2 = "--------------------------------------------------\n";
                                Writen(client[i].fd, tmp2, strlen(tmp2));
                            }
                            else{
                                // not valid cmd
                                
                                display_msg1(client[i].fd, "%d-%02d-%02d %02d:%02d:%02d *** Unknown or incomplete command <%s>\n", recvline);
                                continue;
                            }
                        }
                    }
                    // regular text
                    else{
                        for(int j=1;j<=maxi;j++){
                            if(i!=j)
                                display_msg2(client[j].fd, "%d-%02d-%02d %02d:%02d:%02d <%s> %s\n", cli_names[i], recvline);
                        }
                    }
                }
                if (--nready <= 0) break; /* no more readable descs */
            }
        }
    }
    return 0;
}
// example : nc inp111.zoolab.org 10005
    /*
bash -c 'MAX=1000; I=0; while [ "$I" -lt "$MAX" ]; do I=$((I+1)); (timeout 30 nc localhost 10005 >/dev/null 2>&1 &) done'
	for ( ; ; ) {
		// accept
		clilen = sizeof(cliaddr);
		if ( (connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen)) < 0) {
			if (errno == EINTR)
				continue;		/* back to for() 
			else
				perror("accept error");
		}


		int pid = fork();
		if (pid==0){
			close(listenfd);
			if(str_echo(connfd)<0){
				exit(-1);
			}
			exit(0);
		}
		close(connfd);			/* parent closes connected socket 
	}*/
