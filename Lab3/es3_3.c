#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <inttypes.h>
#include <fcntl.h>

#include <string.h>
#include <time.h>

#include    "../libraries/errlib.h"
#include    "../libraries/sockwrap.h"

#define BUFLEN		128 /* Buffer length */
#define MAX_CHILD	10
#define TIMEOUT		120

/* GLOBAL VARIABLES */
const char *prog_name;

int perform_client_request(int s1, int i);

int main(int argc, char *argv[]) {
	
	int s, s1;
	struct addrinfo *res, hints, *p;
	
	int	bklog = 2;
	struct sockaddr_storage caddr;
	socklen_t addrlen;
	
	/** ADDED **/
	int i;
	int n_child;

	prog_name = argv[0];

	if(argc != 3)
		err_quit("usage: %s <port> <n_child>", prog_name);

	n_child = atoi(argv[2]);
	
	if(n_child > MAX_CHILD)
		err_quit("Number of children must be lower than %d.\n", MAX_CHILD);

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

	/** ADDED **/
	for(i = 0; i < n_child; i++){

		if(fork() == 0) {

			/*Server loop*/
			for(;;){
				printf("(%s - %d) - Waiting for connection...\n", prog_name, i);
		
				/*Accept*/
				addrlen = sizeof(struct sockaddr_storage);
				s1 = Accept(s, (struct sockaddr *) &caddr, &addrlen); 

				showAddr("Accepted connection from", (struct sockaddr_in*) &caddr);
			
				if(perform_client_request(s1, i))
					printf("(%s - %d) - Connection closed by the client!\n", prog_name, i);
				else
					printf("(%s - %d) - Timeout expired!\n", prog_name, i);
	
				Close(s1);
			}		
			return 0;
		} 
	}

	for(i = 0; i < n_child; i++)
		wait(NULL);

	Close(s);
	return(0);
}

int perform_client_request(int s1, int i) {

	char filename[BUFLEN-7];
	struct stat fileinfo;
	uint32_t filesize, timestamp;
	FILE *fp;
	char c;

	ssize_t n;
	char buf[BUFLEN];
	char rbuf[BUFLEN];

	struct timeval tval;
	fd_set cset;

	printf("\n(%s) Waiting a command...\n", prog_name);

	FD_ZERO(&cset); //Clears the set
    FD_SET(s1, &cset); //Add a file descriptor to a set
    tval.tv_sec = TIMEOUT; //Timeout interval
    tval.tv_usec = 0;

    while(Select(FD_SETSIZE, &cset, NULL, NULL, &tval)) { //Waits until the file descriptor is ready

    	n = readline_unbuffered(s1, buf, BUFLEN-1);

		/* Nothing read from client */
		if(n == 0)
	       	return 1;
		
	    cleanString(buf);
	    printf("(%s %d) - Received: %s\n", prog_name, i, buf);

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

   			if(writen(s1, "+OK\r\n", 5) != 5) {
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

   			while((c = fgetc(fp)) != EOF) {
				if(writen(s1, &c, sizeof(char)) != sizeof(char)) {
	    	   		err_msg("(%s) ERROR! Client Unreacheable!\n", prog_name);
						continue;
   				}
   			}
   			printf("(%s %d) - The file %s was sended!\n",prog_name, i, filename);
   			fclose(fp);

   		/* quit command */
   		} else if(strncmp(buf, "QUIT", 4) == 0) {
   			break;

   		/* not allowed command*/
   		} else {
   			sprintf(rbuf, "-ERR\r\n");
   			if(writen(s1, rbuf, 6) != 6) 
	    		err_msg("(%s) ERROR! Client Unreacheable!\n", prog_name);
   			continue;
   		}	

   		printf("\n(%s %d) - Waiting a command...\n", prog_name, i);	

    }
    return 0;
}			
