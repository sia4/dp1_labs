#include     <stdlib.h>
#include     <string.h>
#include 	 <fcntl.h>
#include 	 <errno.h>
#include     <inttypes.h>

#include     "../libraries/errlib.h"
#include     "../libraries/sockwrap.h"

#define BUFLEN 	32  /* BUFFER LENGTH */
#define TIMEOUT 3  /* TIMEOUT (seconds) */

/* GLOBAL VARIABLES */
char *prog_name;

int main(int argc, char *argv[]) {

	/* Connection params */
	struct addrinfo *res, hints, *p;
    int	s;

	char buf[BUFLEN];
	char rbuf[BUFLEN];

    /* Program params */
    size_t len, n;
    fd_set cset;
    struct timeval tval;
    int attempt;

    prog_name = argv[0];

	if(argc != 4)
		err_quit ("usage: %s <address> <port> <name>\n", prog_name);

	if(strlen(argv[3]) >= BUFLEN)
		err_quit ("(%s) <name> must be lower than 32 characters.\n", prog_name);

	strcpy(buf, argv[3]);

	/* use getaddrinfo to parse address and port number */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
	hints.ai_socktype = SOCK_DGRAM;

	Getaddrinfo(argv[1], argv[2], &hints, &res);
	printf("(%s) Address and port parsed with getaddrinfo.\n", prog_name);

	/* loop through all the results and connect to the first we can */
	for(p = res; p != NULL; p = p->ai_next) {
	    if ((s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) != -1) {
	    	printf("(%s) Socket created. Socket number: %d\n",prog_name, s);
	        break;
	    }
	}
    
	if (p == NULL) {
	    // looped off the end of the list with no successful bind
	    err_quit("(%s) Failed to bind socket\n", prog_name);
	    exit(2);
	}

	freeaddrinfo(res);
	
	/* Send datagram */
	len = strlen(buf);
    n = sendto(s, buf, len, 0, p->ai_addr, p->ai_addrlen);
    if (n != len) {
		printf("(%s) sendto() error.\n", prog_name);
		close(s);
		exit(0);
    }

    attempt = 0;
    FD_ZERO(&cset);
    FD_SET(s, &cset);
    tval.tv_sec = TIMEOUT;
    tval.tv_usec = 0;

    /* Try 5 times to read answer */
    while(attempt < 5) {
	    printf("(%s) Waiting for response...\n", prog_name);

     	FD_ZERO(&cset);
	    FD_SET(s, &cset);
	    tval.tv_sec = TIMEOUT;
	    tval.tv_usec = 0;
	    n = Select(FD_SETSIZE, &cset, NULL, NULL, &tval);

	    if(n == -1)
			err_quit("select() failed");
			
	    if (n > 0) {
			/* receive datagram */
			n=recvfrom(s, rbuf, BUFLEN-1, 0, p->ai_addr, &p->ai_addrlen);
		    if (n != -1) {
				cleanString(rbuf);
				showAddr("Received response from", (struct sockaddr_in *)p->ai_addr);
				printf(": [%s]\n", rbuf);
			} else 
				printf("(%s) recvfrom() error.\n", prog_name);
			break;
		} else 
			printf("(%s) Attempt number %d. No response received after %d seconds\n",prog_name, attempt+1, TIMEOUT);
		attempt++;
	}

	Close(s);
	exit(0);
}