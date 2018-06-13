#include     <stdlib.h>
#include     <string.h>
#include     <inttypes.h>
#include     "../libraries/errlib.h"
#include     "../libraries/sockwrap.h"

char *prog_name;

int main(int argc, char *argv[]) {

    uint16_t tport_n, tport_h;	/* server port number (net/host ord) */

    int s;
    int result;
    struct sockaddr_in saddr;	/* server address structure */
    struct in_addr sIPaddr; 	/* server IP addr. structure */

	prog_name = argv[0];

	if(argc != 3)
		err_quit ("usage: %s <address> <port>\n", prog_name);

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
    printf("done.\n");

	close(s);
	exit(0);
}