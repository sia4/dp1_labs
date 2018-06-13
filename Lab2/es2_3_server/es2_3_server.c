#include 	<stdio.h>
#include 	<stdlib.h>
#include 	<stdint.h>
#include 	<unistd.h>
#include 	<errno.h>
#include 	<ctype.h>

#include 	<sys/types.h>
#include 	<sys/socket.h>
#include 	<sys/stat.h>
#include 	<sys/wait.h>
#include 	<arpa/inet.h>
#include 	<netinet/in.h>
#include 	<inttypes.h>
#include 	<fcntl.h>

#include 	<string.h>
#include 	<time.h>

#include    "../../libraries/errlib.h"
#include	"../../libraries/sockwrap.h"

#define BUFLEN		128 /* Buffer length */

/* GLOBAL VARIABLES */
const char *prog_name;

int main(int argc, char const *argv[]) {
	
	int s, s1;
	struct addrinfo *res, hints, *p;
	int	bklog = 2;
	struct sockaddr_storage caddr;
	socklen_t addrlen;
	ssize_t n;
	char buf[BUFLEN];
	char rbuf[BUFLEN];

	/* Program params */
	char filename[128-7];
	struct stat fileinfo;
	uint32_t filesize, timestamp;
	FILE *fp;
	char c;

	prog_name = argv[0];

	if(argc != 2)
		err_quit("usage: %s <port>", prog_name);

	/* use getaddrinfo to parse address and port number */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	Getaddrinfo(NULL, argv[1], &hints, &res);
	printf("Address and port parsed with getaddrinfo.\n");

	/* loop through all the results and connect to the first we can */
	for(p = res; p != NULL; p = p->ai_next) {
	    if ((s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) != -1) {
	    	printf("Socket created. Socket number: %d\n",s);
	        break;
	    }
	}
    
	if (p == NULL) {
	    // looped off the end of the list with no successful bind
	    err_quit("Failed to bind socket\n");
	    exit(2);
	}

	/* Binding */
	Bind(s, p->ai_addr, p->ai_addrlen);
    printf("Bind done.\n");

	/* listen */
    printf ("Listening at socket %d with backlog = %d \n",s,bklog);
    Listen(s, bklog);
    printf("Listen done.\n");

  	freeaddrinfo(res); // all done with this structure
			

    /* Main Loop */
    while(1) {
    	/* accept next connection */
		addrlen = sizeof(struct sockaddr_storage);
		s1 = Accept(s, (struct sockaddr *) &caddr, &addrlen);
		showAddr("Accepted connection from", (struct sockaddr_in*) &caddr);
		printf("new socket: %u\n",s1);

		printf("\n(%s) Waiting a command...\n", prog_name);

		/* Command from client loop */
		while((n = readline_unbuffered(s1, buf, BUFLEN-1)) >= 0) {

			/* Nothing read from client */
			if(n == 0) {
				printf("Connection closed by party on socket %d\n",s);
		       	close(s1);
		       	break;
			}
       		printf("[%s]\n",buf);

       		/* GET command */
       		if(strncmp(buf, "GET", 3) == 0) {
       			sscanf(buf, "%*s %s", filename);
       			printf("filename = %s\n", filename);

       			/* Read file info */
       			if(stat(filename, &fileinfo) < 0) {
       				err_msg("(%s) ERROR! Cannot read file info!\n", prog_name);
       				sprintf(rbuf, "-ERR\r\n");
	       			if(writen(s1, rbuf, 6) != 6)
			    	   err_msg("(%s) ERROR! Client Unreacheable!\n", prog_name);
       				continue;
       			}

       			filesize = fileinfo.st_size;
       			timestamp = fileinfo.st_mtime;

       			printf("filesize = " "%" SCNu32 "\n", filesize);
       			printf("timestamp = " "%" SCNu32 "\n", timestamp);

       			/* Open file */
       			fp = fopen(filename, "r");
       			if(fp == NULL) {
       				err_msg("(%s) ERROR! Cannot open file!\n", prog_name);
					sprintf(rbuf, "-ERR\r\n");
	       			if(writen(s1, rbuf, 6) != 6)
			    	   err_msg("(%s) ERROR! Client Unreacheable!\n", prog_name);
       				continue;
       			} 

       			sprintf(rbuf, "+OK\r\n");
       			if(writen(s1, rbuf, 5) != 5) {
					err_msg("(%s) ERROR! Client Unreacheable!\n", prog_name);
       				continue;
       			}
       			filesize = htonl(filesize);
       			timestamp = htonl(timestamp);
       			if(writen(s1, &filesize, sizeof(uint32_t)) != sizeof(uint32_t)) {
					err_msg("(%s) ERROR! Client Unreacheable!\n", prog_name);
       				continue;
       			}
       			if(writen(s1, &timestamp, sizeof(uint32_t)) != sizeof(uint32_t)) {
       				err_msg("(%s) ERROR! Client Unreacheable!\n", prog_name);
       				continue;
       			}

       			/*while((c = fgetc(fp)) != EOF) {
					if(writen(s1, &c, sizeof(char)) != sizeof(char)) {
		    	   		err_msg("(%s) ERROR! Client Unreacheable!\n", prog_name);
   						continue;
       				}
       			}*/
       			int endS = 0;
       			int nC;
				while(!endS) {
					//printf("%c", c);
					if((nC = fread(buf, sizeof(char), BUFLEN-1, fp)) != BUFLEN-1)
						endS = 1;
					if(writen(s1, buf, nC) !=nC) {
				   		err_quit("(%s) ERROR! Client Unreacheable!\n", prog_name);
					}
				}
       			printf("File sent.\n");
       			fclose(fp);

       		/* quit command */
       		} else if(strncmp(buf, "QUIT", 4) == 0) {
       			err_msg("(%s) Connection closed by the client!\n", prog_name);
       			close(s1);
       			break;

       		/* not allowed command*/
       		} else {
       			sprintf(rbuf, "-ERR\r\n");
       			if(writen(s1, rbuf, 6) != 6)
		    		err_msg("(%s) ERROR! Client Unreacheable!\n", prog_name);
       			close(s1);
       			break;
       		}	

       		printf("\n(%s) Waiting a command...\n", prog_name);
		}
    }
	
    close(s);
	return 0;
}