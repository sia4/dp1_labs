#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    <inttypes.h>
#include 	<errno.h>

#include    "../libraries/errlib.h"
#include    "../libraries/sockwrap.h"

#define BUFLEN 		32
#define HOST_N 		10
#define MAX_CONN	3
#define MAX_LEN		32
/* GLOBAL VARIABLES */
char *prog_name;

typedef struct hosts {
	char caddr[MAX_LEN];
	int count;
} Hosts;

int main (int argc, char *argv[]) {

	/* Connestion variables */
	struct addrinfo *res, hints, *p;
    int	s;
 	struct sockaddr_storage from;
    socklen_t addrlen;

    /* Program params */
    size_t n;
	char buf[BUFLEN];
	Hosts hosts[HOST_N];
	int n_host, nH, pos;
	int i, found;
	char chost[MAX_LEN];
	char cservice[MAX_LEN];

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
	printf("(%s) Bind done.\n", prog_name);

	freeaddrinfo(res);

	n_host = 0;
	nH = 0;

	/* Main server loop */
	while(1){
		printf("(%s) Waiting for a message...\n", prog_name);
		addrlen = sizeof(struct sockaddr_storage);
		n = recvfrom(s, buf, BUFLEN-1, 0, (struct sockaddr *)&from, &addrlen);

        if (n != -1) {
            buf[n] = '\0';
            printf("(%s) Received: [%s]\n", prog_name, buf);

			if(getnameinfo((struct sockaddr *)&from, sizeof(from), chost, 
				sizeof(chost), cservice, sizeof(cservice), 
				NI_NUMERICHOST|NI_NUMERICSERV) != 0) {
				printf("(%s) Error while reading address\n", prog_name);
				Close(s);
				return(-1);
			}

			/*Look if the client is in the buffer*/
			found = 0;
			pos = 0;
			for(i = 0; i < nH && !found; i++) {
				if(strcmp(hosts[i].caddr, chost) == 0) {
					hosts[i].count++;
					found = 1;
					pos = i;
				}
			}

			/*Add a new element in the buffer*/
			if(!found) {
				pos = n_host;
				strcpy(hosts[n_host].caddr, chost);
				hosts[n_host].count = 0;
				n_host = (n_host+1) % HOST_N;

				if(nH < HOST_N)
					nH++;
			}

			/*Performed actions only if the number of request is less than MAX_CONN*/
			if(hosts[pos].count < MAX_CONN) {
		    	if(sendto(s, buf, n, 0, (struct sockaddr *)&from, addrlen) != n)
					err_msg("(%s) sendto() error \n", prog_name);
				else
					printf("(%s) Reply sent.\n", prog_name);
			} else
				printf("(%s) Error. Number of connections exceeded.\n", prog_name);

		}
	}

	Close(s);
	return(0);

}