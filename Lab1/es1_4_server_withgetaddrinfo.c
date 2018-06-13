#include    <stdlib.h>
#include    <string.h>
#include    <inttypes.h>
#include    "../libraries/errlib.h"
#include    "../libraries/sockwrap.h"

#define BUFLEN 128

/* GLOBAL VARIABLES */
char *prog_name;

int main (int argc, char *argv[]) {

	/* Connestion variables */
	struct addrinfo *res, hints, *p;
    int	s;

    /* Program params */
    size_t n;
	char buf[BUFLEN];

    prog_name = argv[0];

	if(argc != 2)
		err_quit ("usage: %s <port>\n", prog_name);

	/* use getaddrinfo to parse address and port number */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	Getaddrinfo(NULL, argv[1], &hints, &res);
	printf("Address and port parsed with getaddrinfo.\n");

	/* loop through all the results and connect to the first we can */
	for(p = res; p != NULL; p = p->ai_next) {
	    if ((s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) != -1) {
	    	printf("(%s) Socket created. Socket number: %d\n",prog_name, s);
	        break;
	    }
	}
    
	if (p == NULL) {
	    // looped off the end of the list with no successful bind
	    err_quit("(%s) Failed to bind socket.\n", prog_name);
	    exit(2);
	}

	/* Binding */
	Bind(s, p->ai_addr, p->ai_addrlen);
	printf("(%s) Bind done.\n", prog_name);

	freeaddrinfo(res);

	/* Main server loop */
	while(1){
		printf("(%s) Waiting for a message...\n", prog_name);
		/* Read message */
		n = recvfrom(s, buf, BUFLEN-1, 0, p->ai_addr, &p->ai_addrlen);
        if (n != -1) {
            cleanString(buf);
	    	showAddr("Received message from", (struct sockaddr_in *) p->ai_addr);
	    	printf(": [%s]\n", buf);

	    	/* Send message */
	    	if(sendto(s, buf, n, 0, p->ai_addr, p->ai_addrlen) != n)
				err_msg("(%s) Write error while replying\n", prog_name);
	    else
			printf("(%s) Reply sent.\n", prog_name);
		}
	}

	Close(s);
	return(0);
}