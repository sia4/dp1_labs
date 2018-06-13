#include     <stdlib.h>
#include     <string.h>
#include     <inttypes.h>

#include     "../libraries/errlib.h"
#include     "../libraries/sockwrap.h"

#define BUFLEN 128
char *prog_name;

int main(int argc, char *argv[]) {

    uint16_t tport_n, tport_h;	/* server port number (net/host ord) */

    int s;
    int result;
    struct sockaddr_in saddr;	/* server address structure */
    struct in_addr sIPaddr; 	/* server IP addr. structure */

    /* exercise params */
    uint16_t res;
    char buf[BUFLEN];			/* transmission buffer */
    char rbuf[BUFLEN];			/* reception buffer */
    size_t len;

	prog_name = argv[0];

	if(argc != 3)
		err_quit ("usage: %s <address> <port>\n", prog_name);

    /* Saving addr and port values */
	result = inet_aton(argv[1], &sIPaddr);
    if (!result)
	err_quit("Invalid address");

	if (sscanf(argv[2], "%" SCNu16, &tport_h)!=1)
	err_quit("Invalid port number");
    tport_n = htons(tport_h);

    /* create the socket */
    printf("Creating socket\n");
    s = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    printf("done. Socket fd number: %d\n",s);

    /* prepare address structure */
    bzero(&saddr, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port   = tport_n;
    saddr.sin_addr   = sIPaddr;

    /* connect */
    showAddr("Connecting to target address", &saddr);
    Connect(s, (struct sockaddr *) &saddr, sizeof(saddr));
    printf("Connection done.\n");

    /* main client loop */
    while(1) {

    	buf[0] = '\0';
    	rbuf[0] = '\0';
    	
    	/* read from stdin */
    	mygetline(buf, BUFLEN, "Enter two integer number ('close' or 'stop' to close connection):\n");
    	if(iscloseorstop(buf))
    		break;

    	/* send params to server */
    	sprintf(buf, "%s\r\n", buf);
    	len = strlen(buf);
		if(writen(s, buf, len) != len) {
		    printf("Write error\n");
		    break;
		}

		/* read from server */
		printf("waiting for response...\n");
		result = Readline(s, rbuf, BUFLEN);
		if (result <= 0) {
		     printf("Read error. Connection closed.\n");
		     close(s);
		     exit(1);
		} else {
			if(rbuf[0] >= '0' && rbuf[0] <= '9'){
				sscanf(rbuf, "%" SCNu16, &res);
				printf("Received answer from socket: " "%" SCNu16 "\n", res);
			} else {
			    printf("%s\n", rbuf);
			}
		}
    }

	close(s);
	exit(0);
}