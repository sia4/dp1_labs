#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <inttypes.h>
#include <fcntl.h>
#include "types.h"

#include <rpc/xdr.h>

#include <string.h>
#include <time.h>

#include "../../libraries/errlib.h"
#include "../../libraries/sockwrap.h"

#define LISTENQ 15
#define MAXBUFL 255

#define QT "QUIT\r\n"

#define MAX_UINT16T 0xffff

#ifdef TRACE
#define trace(x) x
#else
#define trace(x)
#endif


char *prog_name;

int receivingdata(int socketDescr, char *filename){

	int nsend, nread, i;
	char buf[MAXBUFL+1];
	int filedescr;
	uint32_t filesizeNet, filesize, timestampNet, timestamp;

	snprintf(buf, MAXBUFL, "GET %s\r\n", filename);
	/*sending the buffer with the values*/
	nsend = sendn(socketDescr, buf, strlen(buf), MSG_NOSIGNAL); // nsend return how many bytes are sent
	
	if(nsend != strlen(buf)){
		printf("Error sending %d bytes to the server\n", (int)strlen(buf));	
		return -1;
	}
	else printf("Sent the command: %s to the server\n", buf);
	
	nread = readn(socketDescr, buf, 5);
	
	if(nread < 0){
		printf("Error receiving data\n");
		return(0);
	} else if(strncmp(buf, "-ERR", 4) == 0){ // ERR case
		nread = readn(socketDescr, buf, 1);
		printf("Error received from server\n");
		return(0);
	} else if(strncmp(buf, "+OK\r\n", 5) == 0){ // OK case
		printf("Ok received from the server\n");
		nread = readn(socketDescr, &filesizeNet, 4);
		filesize = ntohl(filesizeNet);
		nread = readn(socketDescr, &timestampNet, 4);
		timestamp = ntohl(timestampNet);
		printf("Received filesize and timestamp: %d, %d\n", filesize, timestamp);
		
		/*opening the file */
		if((filedescr = open(filename, O_RDWR | O_CREAT, 0777)) == -1){ // O_CREAT creates the file if it does not exist
			printf("Error creating the file\n");
			return(0);
		}
		strcpy(buf, "");	
	
		nread = 0;
		for(i=0; i<filesize; i=i + nread){
                    nread = Recv(socketDescr, buf, MAXBUFL*sizeof(char), 0);
                    if( nread <= 0 ){
                        close(filedescr);
                        printf("(%s) ERROR! During the download of the file!\n", prog_name);
						return(0);
                    }
                    write(filedescr, buf, nread);
                }
		close(filedescr);
		printf("File received!\n");	
		return(0);
	}
	

	return(0);
}

int receivingdata_xdr(int socketDescr, char *filename){

	int filedescr;
	uint32_t  filesize, timestamp;
	message mess_in, mess_out; 
	
	XDR xdrs_w, xdrs_r;
	FILE *fstream_w, *fstream_r;
	
	/* create file fstream_w */
	fstream_w = fdopen(socketDescr, "w");
	if (fstream_w == NULL){
		fprintf(stdout, "(%s) error - fdopen() fstream_w failed\n", prog_name); fflush(stdout);
		xdr_destroy(&xdrs_w);
		xdr_destroy(&xdrs_r);
		return(-1);
	}
	/* creating XDR stream */
	xdrstdio_create(&xdrs_w, fstream_w, XDR_ENCODE);
	
	/* create file fstream_r */
	fstream_r = fdopen(socketDescr, "r");
	if (fstream_r == NULL){
		fprintf(stdout, "(%s) error - fdopen() fstream_r failed\n", prog_name); fflush(stdout);
		xdr_destroy(&xdrs_w);
		xdr_destroy(&xdrs_r);
		return(-1);
	}

	/* creating XDR stream */
	xdrstdio_create(&xdrs_r, fstream_r, XDR_DECODE);
	
	mess_out.message_u.filename=malloc(MAXBUFL*sizeof(char));
	mess_in.message_u.filename=malloc(MAXBUFL*sizeof(char));
	
	memset (&mess_in,0,sizeof(message));
	memset (&mess_out,0,sizeof(message));
	
	if((strcmp(filename, "QUIT") == 0) || (strcmp(filename, "quit") == 0)){
		mess_out.tag=QUIT;
		if(xdr_message(&xdrs_w, &mess_out)==0){
			printf("Error closing connection\n");
			xdr_destroy(&xdrs_w);
			xdr_destroy(&xdrs_r);
			Close(socketDescr);
			exit(0);
		}
		else{
			printf("Connection correctly closed\n");
			xdr_destroy(&xdrs_w);
			xdr_destroy(&xdrs_r);
			Close(socketDescr);
			exit(0);
		}
	}
	mess_out.tag = GET;
	mess_out.message_u.filename=filename;
	
	/*sending the buffer with the values*/
	if(xdr_message(&xdrs_w, &mess_out) == 0){
		printf("Error sending file name\n");
		xdr_destroy(&xdrs_w);
		xdr_destroy(&xdrs_r);
		return(0);
	}
	fflush(fstream_w);
	
	if(xdr_message(&xdrs_r, &mess_in)==0){
		printf("Error receiving data\n");
		xdr_destroy(&xdrs_w);
		xdr_destroy(&xdrs_r);
		return(0);
	} else if(mess_in.tag == ERR){ // ERR case
		printf("Error received from server\n");
		xdr_destroy(&xdrs_w);
		xdr_destroy(&xdrs_r);
		return(0);
	} else if(mess_in.tag == OK){ // OK case
		printf("Ok received from the server\n");
		filesize=mess_in.message_u.fdata.contents.contents_len;
		//filesize = ntohl(filesizeNet);  non va fatto perchè lo fa automaticamente xdr
		timestamp = mess_in.message_u.fdata.last_mod_time;
		//timestamp = ntohl(timestampNet);  on va fatto perchè lo fa automaticamente xdr
		printf("Received filesize and timestamp: %d, %d\n", filesize, timestamp);
		
		/*opening the file */
		if((filedescr = open(filename, O_RDWR | O_CREAT, 0777)) == -1){ // O_CREAT creates the file if it does not exist
			printf("Error creating the file\n");
			xdr_destroy(&xdrs_w);
			xdr_destroy(&xdrs_r);
			return(0);
		}
		
		
        write(filedescr, mess_in.message_u.fdata.contents.contents_val, mess_in.message_u.fdata.contents.contents_len);
        close(filedescr);
		printf("File received!\n");	
		xdr_destroy(&xdrs_w);
		xdr_destroy(&xdrs_r);
		return(0);
	}
	return(0);
}

int main(int argc, char *argv[]){
	
	int xdr_flag=0;
	prog_name = argv[0];
	int socketDescr, err = 0;
	struct sockaddr_in servaddr;
	short port;
	socklen_t servaddrlen = sizeof(servaddr);
	char filename[MAXBUFL+1];
	
	if (argc==3 || argc==4){		
		if(argc==4){
			if(strcmp(argv[1], "-x")==0){
				xdr_flag=1;
				port=atoi(argv[3]);
				err = inet_aton(argv[2], &(servaddr.sin_addr));
			}
		}
		else{
			xdr_flag=0;
			port=atoi(argv[2]);
			err = inet_aton(argv[1], &(servaddr.sin_addr));
		}
	}
	else{
		err_quit("Invalid parameters from command line: usage %s <address> <port>\n", argv[0]);
		exit(0);
	}
	
	socketDescr = Socket(AF_INET, SOCK_STREAM, 0); // create the socket

	/* specify address to connect with */
	if(err == 0){
		printf("Address specification failed\n");
		return -1;
	}
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);

	err = connect(socketDescr, (SA*) &servaddr, servaddrlen);
	if(err == -1){
		trace(err_msg("Connection error\n"));
		return -1;
	} else{ // connection is well performed
		trace(err_msg("Connection performed with server %s : port %d", inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port)));	
	}

	while(1){
		printf("\nEnter the file name you want to get (QUIT to exit):\n");
		scanf("%s", filename);
		printf("Input is: %s\n", filename);
		
		if(xdr_flag==0){
			if((strncmp(filename, "QUIT", 4) ==0) || (strncmp(filename, "quit", 4) == 0)){
				if(sendn(socketDescr, QT, 6, MSG_NOSIGNAL) != 6){
					printf("Error sending quit command\n");
				}
				break;
			}
			else if(receivingdata(socketDescr, filename)==-1){
				printf("Cannot connect to the server! Closing program...\n");
				break; 
			}
		}
		else{
			if(receivingdata_xdr(socketDescr, filename)==-1){
				printf("Cannot connect to the server! Closing program...\n");
				break; 
			}
		}		
	}
	
	Close(socketDescr);
	return 0;
}
