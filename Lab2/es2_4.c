#include     <stdio.h>
#include     <stdlib.h>
#include     <string.h>
#include     <inttypes.h>
#include 	 <rpc/xdr.h>
#include	 <errno.h>

#include     "../libraries/errlib.h"
#include     "../libraries/sockwrap.h"

#define BUFLEN 128
char *prog_name;

int main(int argc, char *argv[]) {

  	struct addrinfo *res, hints, *p;
    int	s;

    /* exercise params */
    char buf[BUFLEN];			/* transmission buffer */
	int num1, num2, resNum; //uint16_t?

	/** XDR **/
	XDR xdr_rs, xdr_ws;		//an XDR stream
    FILE* fp_xdr_w = NULL, *fp_xdr_r = NULL;
	int r1, r2;

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

	    printf("(%s) Socket created. Socket number: %d\n",prog_name, s);

	    if (connect(s, p->ai_addr, p->ai_addrlen) == -1) {
	        Close(s);
	        continue;
	    }

	    printf("(%s) Successfully connected.\n", prog_name);

	    break; // if we get here, we must have connected successfully
	}
    
	if (p == NULL) {
	    // looped off the end of the list with no successful bind
	    err_quit("(%s) Failed to bind socket\n", prog_name);
	    exit(2);
	}

	freeaddrinfo(res); // all done with this structure

	/** XDR **/
    fp_xdr_w = fdopen(s, "w");
    if(fp_xdr_w == NULL)
        err_quit("Impossible to open xdr writing stram!\nERROR: %s\n", strerror(errno));

    fp_xdr_r = fdopen(s, "r");
    if(fp_xdr_r == NULL)
        err_quit("Impossible to open xdr reading stram!\nERROR: %s\n", strerror(errno));


	/** XDR **/
	xdrstdio_create(&xdr_ws, fp_xdr_w, XDR_ENCODE);
    xdrstdio_create(&xdr_rs, fp_xdr_r, XDR_DECODE);

	memset(buf, 0, BUFLEN);
	
	/* read from stdin */
	mygetline(buf, BUFLEN-1, "Enter two integer number\n");

	cleanString(buf);
	num1 = 0; num2 = 0;
    sscanf(buf, "%d %d", &num1, &num2);

	/** XDR **/
	/* send params to server */
	r1 = xdr_int(&xdr_ws, &num1);	
	fflush(fp_xdr_w);			
    r2 = xdr_int(&xdr_ws, &num2);
	fflush(fp_xdr_w);
		
    if (r1 == 0)									
        err_msg("(%s) Error! Op1 not sended!\n", prog_name);         
    if (r2 == 0)									
        err_msg("(%s) Error! Op2 not sended!\n", prog_name);	        
    if((r1==0)||(r2==0))                            
        err_quit("(%s) Error! Transmission error!\n", prog_name);   


	/** XDR **/
	/*read from server */
	printf("waiting for response...\n");
	if( !xdr_int(&xdr_rs, &resNum) ) {		//result	
        printf("Error! Invalid answer!\n");  
		printf("Answer: %d\n", resNum);       
    } else								    		
        printf("(%s) The result is = %d\n", prog_name, resNum);		

    /** XDR **/
    /* Destroy resources */
    xdr_destroy(&xdr_ws);
    xdr_destroy(&xdr_rs);

	printf("(%s) Closing connection.\n", prog_name);
	/* close everything and terminate */
	Close(s);

	fclose(fp_xdr_w);
	fclose(fp_xdr_r);
	
	exit(0);
}