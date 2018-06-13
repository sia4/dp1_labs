#include     <stdlib.h>
#include     <string.h>
#include     <inttypes.h>

#include     "../libraries/errlib.h"
#include     "../libraries/sockwrap.h"

#define BUFLEN 128
char *prog_name;

int main(int argc, char *argv[]) {

	/* Connection vars */
  	struct addrinfo *res, hints, *p;
    int	s;

    /* exercise params */
    uint16_t r;
    char buf[BUFLEN];			/* transmission buffer */
    char rbuf[BUFLEN];			/* reception buffer */
    size_t len;

	prog_name = argv[0];

	if(argc != 3)
		err_quit ("usage: %s <address> <port>\n", prog_name);

	/* use getaddrinfo to parse address and port number */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
	hints.ai_socktype = SOCK_STREAM;
	Getaddrinfo(argv[1], argv[2], &hints, &res);

	printf("(%s) Address and port parsed with getaddrinfo.\n", prog_name);

	/* loop through all the results and connect to the first we can */
	for(p = res; p != NULL; p = p->ai_next) {
	    if ((s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
	        continue;
	    }

	   printf("(%s) Socket created. Socket number: %d\n", prog_name, s);

	    if (connect(s, p->ai_addr, p->ai_addrlen) == -1) {
	        close(s);
	        continue;
	    }

	    printf("(%s) Successfully connected.\n", prog_name);

	    break; // if we get here, we must have connected successfully
	}
    
	if (p == NULL) {
	    // looped off the end of the list with no successful bind
	    err_quit("Failed to bind socket\n");
	    exit(2);
	}

	freeaddrinfo(res); // all done with this structure

	/* main client loop */
    while(1) {

    	memset(buf, 0, BUFLEN);
    	memset(rbuf, 0, BUFLEN);
    	
    	/* read from stdin */
    	mygetline(buf, BUFLEN-1, "Enter two integer number ('close' or 'stop' to close connection):\n");
    	if(iscloseorstop(buf))
    		break;

    	/* send params to server */
    	cleanString(buf);
    	sprintf(buf, "%s\r\n", buf);
    	len = strlen(buf);
		if(writen(s, buf, len) != len) {
		    err_msg("(%s) error - writen() failed", prog_name);
		    break;
		}

		/* read from server */
		printf("(%s) Waiting for response...\n", prog_name);
		if(Readline(s, rbuf, BUFLEN-1) <= 0){
		     printf("Read error. Connection closed.\n");
		     close(s);
		     exit(1);
		} else if(rbuf[0] >= '0' && rbuf[0] <= '9'){
			sscanf(rbuf, "%" SCNu16, &r);
			printf("(%s) Received answer from server: " "%" SCNu16 "\n", prog_name, r);
		} else {
		    printf("(%s) Received answer from server: %s\n", prog_name, rbuf);
		}
		
    }

	printf("(%s) Closing connection.\n", prog_name);
	/* close everything and terminate */
	Close(s);
	exit(0);
}