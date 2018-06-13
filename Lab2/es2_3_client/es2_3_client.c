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

#include     "../../libraries/errlib.h"
#include     "../../libraries/sockwrap.h"

#define BUFLEN 128
char *prog_name;

int read_file(char *filename, uint32_t n_read, int s, char *rbuf);

int main(int argc, char *argv[]) {

  	struct addrinfo *res, hints, *p;
    int	s;

    /* exercise params */
    char buf[BUFLEN];
    char filename[BUFLEN];			/* transmission buffer */
    char rbuf[BUFLEN];			/* reception buffer */
    size_t len;
	uint32_t filesize, timestamp;
	int result;
	int end;

	prog_name = argv[0];
	
	if(argc != 3)
		err_quit ("usage: %s <address> <port>\n", prog_name);

	/* use getaddrinfo to parse address and port number */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
	hints.ai_socktype = SOCK_STREAM;
	Getaddrinfo(argv[1], argv[2], &hints, &res);
	printf("Address and port parsed with getaddrinfo.\n");

	/* loop through all the results and connect to the first we can */
	for(p = res; p != NULL; p = p->ai_next) {
	    if ((s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
	        continue;
	    }

	    printf("Socket created. Socket number: %d\n",s);

	    /* Connection */
	    if (connect(s, p->ai_addr, p->ai_addrlen) == -1) {
	        Close(s);
	        continue;
	    }

	    printf("Successfully connected.\n");

	    break; // if we get here, we must have connected successfully
	}
    
	if (p == NULL) {
	    // looped off the end of the list with no successful bind
	    err_msg("(%s) ERROR! Failed to bind socket!\n", prog_name);
	    exit(2);
	}

	freeaddrinfo(res); // all done with this structure

	/* main client loop */
	end = 0;
    while(!end) {

    	memset(buf, 0, BUFLEN);
    	memset(rbuf, 0, BUFLEN);

    	/* read from stdin */
    	mygetline(buf, BUFLEN-1, "Enter file name ('quit' to close connection):\n");
    	if((strncmp(buf, "QUIT", 4)) == 0 || (strncmp(buf, "quit", 4) == 0)) {
    		/* send quit message */
    		sprintf(buf, "QUIT\r\n");
			if(writen(s, buf, 6) != 6)
			    err_msg("(%s) ERROR! Server Unreacheable!\n", prog_name);
			    
    		end = 1;
    	} else {

	    	strcpy(filename, buf);

	    	/* send params to server */
	    	sprintf(buf, "GET %s\r\n", filename);
	    	len = strlen(buf);
			if(writen(s, buf, len) != len) {
			    err_msg("(%s) ERROR! Server Unreacheable!\n", prog_name);
			    continue;
			}

			/* read from server */
			printf("Waiting for response...\n");
			result = readn(s, rbuf, 5);
			if (result <= 0) {
			    err_msg("(%s) ERROR! Server Unreacheable!\n", prog_name);
			    continue;
			}

			if(strncmp(rbuf, "+OK\r\n", 5) == 0) {
				result = readn(s, &filesize, sizeof(uint32_t));
				if (result <= 0) {
				    err_msg("(%s) ERROR! Server Unreacheable!\n", prog_name);
			    	continue;
				}
				result = readn(s, &timestamp, sizeof(uint32_t));
				if (result <= 0) {
				    err_msg("(%s) ERROR! Server Unreacheable!\n", prog_name);
			    	continue;
				}

				filesize = ntohl(filesize);
				timestamp = ntohl(timestamp);


				if(read_file(filename, filesize, s, rbuf) == -1)
					continue;

				printf("File transfer completed.\n");
				printf("Filename = %s;\nSize = " "%" SCNu32 ";\nLast modification timestamp = " "%" SCNu32 ".\n", filename, filesize, timestamp);
				
			} else if(strncmp(rbuf, "-ERR\r\n", 6) == 0) {
				printf("(%s) ERROR! The file probably doesn't exist!\n\n --- Usage ---\n", prog_name);
                printf("<filename>: allows to you to download a file \nQUIT: close the program.\n\n");
			} else {
				printf("(%s) ERROR!!!\n\n --- Usage ---\n", prog_name);
                printf("<filename>: allows to you to download a file \nQUIT: close the program.\n\n");
			}		
		}
    }

	Close(s);
	printf("\nQUIT received, socket closed!\n");
	/* close everything and terminate */
	exit(0);
}

int read_file(char *filename, uint32_t n_read, int s, char *rbuf) {

	int result;
	FILE *fp;

	fp = fopen(filename, "w");
	if(fp == NULL) {
		err_msg("(%s) ERROR! Can't open file!\n", prog_name);
		return -1;
	}
	printf("File created.\n");

	/* read and store file */
	while(n_read > 0) {
		if(BUFLEN < n_read) {
			result = readn(s, rbuf, BUFLEN-1);
			//printf("%s\n", rbuf);
			if (result <= 0) {
				fclose(fp);
			    err_msg("(%s) ERROR! A Server Unreacheable!\n", prog_name);
			    return -1;
			}
			n_read -= BUFLEN-1;
		} else {
			memset(rbuf, 0, BUFLEN);
			result = readn(s, rbuf, n_read);
			//printf("%s\n", rbuf);
			if (result <= 0) {
				fclose(fp);
				err_msg("(%s) ERROR! B Server Unreacheable!\n", prog_name);
			    return -1;
			}
			n_read = 0;
		}
		fprintf(fp, "%s", rbuf);
	}

	fclose(fp);

	return 0;
}