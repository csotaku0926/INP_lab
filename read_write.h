#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#define MAX(a,b) (a>b?a:b)

static int	read_cnt;
static char	*read_ptr;
static char	read_buf[1024];
int MAXLINE = 1024;
// read line functions
static ssize_t
my_read(int fd, char *ptr)
{

	if (read_cnt <= 0) {
again:
		if ( (read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0) {
			if (errno == EINTR)
				goto again;
			return(-1);
		} else if (read_cnt == 0)
			return(0);
		read_ptr = read_buf;
	}

	read_cnt--;
	*ptr = *read_ptr++;
	return(1);
}

ssize_t
readline(int fd, void *vptr, size_t maxlen)
{
	ssize_t	n, rc;
	char	c, *ptr;

	ptr = vptr;
	for (n = 1; n < maxlen; n++) {
		if ( (rc = my_read(fd, &c)) == 1) {
			*ptr++ = c;
			if (c == '\n')
				break;	/* newline is stored, like fgets() */
		} else if (rc == 0) {
			*ptr = 0;
			return(n - 1);	/* EOF, n - 1 bytes were read */
		} else
			return(-1);		/* error, errno set by read() */
	}

	*ptr = 0;	/* null terminate like fgets() */
	return(n);
}

ssize_t
Readline(int fd, void *ptr, size_t maxlen)
{
	ssize_t		n;

	if ( (n = readline(fd, ptr, maxlen)) < 0)
		perror("readline error");
	return(n);
}

// write line functions
ssize_t						/* Write "n" bytes to a descriptor. */
writen(int fd, const void *vptr, size_t n)
{
	size_t		nleft;
	ssize_t		nwritten;
	const char	*ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
			if (nwritten < 0 && errno == EINTR)
				nwritten = 0;		/* and call write() again */
			else
				return(-1);			/* error */
		}

		nleft -= nwritten;
		ptr   += nwritten;
	}
	return(n);
}
/* end writen */

void
Writen(int fd, void *ptr, size_t nbytes)
{
	if (writen(fd, ptr, nbytes) != nbytes)
		return;
}

int
str_echo_numadd(int sockfd)
{
	long		arg1, arg2;
	ssize_t		n;
	char		line[MAXLINE];

	for ( ; ; ) {
		if ( (n = Readline(sockfd, line, MAXLINE)) == 0)
			return 1;		/* connection closed by other end */

		if (sscanf(line, "%ld%ld", &arg1, &arg2) == 2)
			snprintf(line, sizeof(line), "%ld\n", arg1 + arg2);
		else
			snprintf(line, sizeof(line), "input error\n");

		n = strlen(line);
		if(writen(sockfd, line, n)<0){
			perror("writen error: ");
			return -1;
		}
	}
	return 0;
}


int
str_echo(int sockfd)
{
	long		arg1, arg2;
	ssize_t		n;
	char		line[MAXLINE];

	for ( ; ; ) {
		if ( (n = readline(sockfd, line, MAXLINE)) == 0){
			printf("readline ends permaturely\n");
			return 0;		/* connection closed by other end */
		}
		else if(n<0){
			perror("readline error: ");
			return -1;
		}
		
		n = strlen(line);
		if(writen(sockfd, line, n)<0){
			perror("writen error: ");
			return -1;
		}
	}
	return 0;
}

int str_cli(FILE *fp, int sockfd)
{
	char	sendline[MAXLINE], recvline[MAXLINE];

	while (fgets(sendline, MAXLINE, fp) != NULL) {

		if(writen(sockfd, sendline, strlen(sendline))<0){
			perror("writen error: ");
			return -1;
		}

		if (Readline(sockfd, recvline, MAXLINE) == 0){
			printf("str_cli: server terminated prematurely");
			return -1;
		}

		if(fputs(recvline, stdout)<0){
			perror("fput error: ");
			return -1;
		}
	}
	return 0;
}

int str_cli_select(FILE* fp, int sockfd){
    int maxline = 1024;
    char sendline[maxline], recvline[maxline];
    // fd_set
    fd_set rset;
	int stdineof = 0;
	FD_ZERO(&rset);
    // writen
    for(;;){
		if(stdineof == 0)
        	FD_SET(fileno(fp), &rset);
        FD_SET(sockfd, &rset);
        int maxfdp1 = MAX(fileno(fp), sockfd) + 1;
        if(select(maxfdp1, &rset, NULL, NULL, NULL)<0){
            perror("select error: ");
            return -1;
        }

        // if sockfd readable
        if(FD_ISSET(sockfd, &rset)){
            
			// when user press ^C
            if(Readline(sockfd, recvline, maxline) == 0){
                // normal termination by client
				if(stdineof==1)
					return 0;
				else{
					printf("server ends permaturely!\n");
					return -1;
				}
            }
            if(fputs(recvline, stdout)<0){
                perror("fput error: ");
                return -1;
            }
        }
        // if stdin readable
        if(FD_ISSET(fileno(fp), &rset)){
            if(fgets(sendline, maxline, fp)!=NULL){
                Writen(sockfd, sendline, strlen(sendline));
            }
            else{ 
				// originally firect return
				stdineof = 1;
				// shutdown : s, how
				if(shutdown(sockfd, SHUT_WR)<0){ // end write part, send FIN
					perror("shutdown error: ");
					return -1;
				}
				FD_CLR(fileno(fp), &rset); // stop to write
				continue;
            }
        }
    }
	return 0;
}