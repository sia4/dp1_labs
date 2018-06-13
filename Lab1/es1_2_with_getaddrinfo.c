#include     <stdio.h>
#include     <stdlib.h>
#include     <string.h>
#include     <inttypes.h>

#include     "../libraries/errlib.h"
#include     "../libraries/sockwrap.h"

char *prog_name;

int main(int argc, char *argv[]) {

	/* Connection vars */
  	struct addrinfo *res, hints, *p;
    int	s;

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
	    err_quit("(%s) Failed to bind socket.\n", prog_name);
	    exit(2);
	}

	freeaddrinfo(res); // all done with this structure

	printf("(%s) Closing connection.\n", prog_name);
	/* close everything and terminate */
	close(s);
	exit(0);
}