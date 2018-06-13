#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <inttypes.h>
#include <signal.h>
#include <fcntl.h>

#include <rpc/xdr.h>

#include <string.h>
#include <time.h>
#include "types.h"

#include "../../libraries/errlib.h"
#include "../../libraries/sockwrap.h"

#define LISTENQ 15
#define MAXBUFL 255
#define BLKSIZE 1024

#define ERROR "-ERR\r\n"


char *prog_name;
int numServ;
pid_t pid[10]={0};		

void signalHandler(int sig){
	int i; 
	for(i=0; i<numServ; i++){
		kill(pid[i], SIGINT);
		waitpid(pid[i], NULL, WNOHANG);
	}	
	exit(0);
}

int transmitdata_xdr(int connfd){

	int file;
	int resto,nread;
	uint32_t filesize;
	uint32_t timestamp;
	message mess_in, mess_out;
	char buf[BLKSIZE+1];
	tagtype tag; 
	struct stat filestat;
	XDR xdrs_w, xdrs_r;
	FILE *fstream_r;
			
	while(1){
	
		/* create buffer for xdr encoding */
		xdrmem_create(&xdrs_w, buf, BLKSIZE, XDR_ENCODE);
		mess_out.message_u.filename=malloc(MAXBUFL*sizeof(char));
	
		/* create file fstream_r */
		fstream_r = fdopen(connfd, "r");
		if (fstream_r == NULL){
			fprintf(stdout, "(%s) error - fdopen() fstream_r failed\n", prog_name); fflush(stdout);
			return(-1);
		}

		/* creating XDR stream */
		xdrstdio_create(&xdrs_r, fstream_r, XDR_DECODE); //This link socket with xdr stream
	
		memset (&mess_in,0,sizeof(message));
		memset (&mess_out,0,sizeof(message));	
	
		printf("Waiting for commands...\n");
		
		//reading command by the client
		if (xdr_message(&xdrs_r, &mess_in)==0){
			printf("(%s) --- cannot read the message sent by the client\n", prog_name);
			xdr_destroy(&xdrs_w);
			xdr_destroy(&xdrs_r);
			return(0);
		}
		else{
			fprintf(stdout, "(%s) --- received string: %s\n",prog_name, mess_in.message_u.filename); fflush(stdout);
			if(mess_in.tag==GET){
				if((file = open( mess_in.message_u.filename, O_RDONLY)) == -1){
					mess_out.tag=ERR;
					xdr_message(&xdrs_w, &mess_out);
					sendn(connfd, buf, xdr_getpos(&xdrs_w), MSG_NOSIGNAL);
					printf("Cannot open the file\n");
					xdr_destroy(&xdrs_w);
					xdr_destroy(&xdrs_r);
					continue;	
				}
				if(stat(mess_in.message_u.filename, &filestat) == -1){
					mess_out.tag=ERR;
					xdr_message(&xdrs_w, &mess_out);
					sendn(connfd, buf, xdr_getpos(&xdrs_w), MSG_NOSIGNAL);
					printf("Cannot stat the file\n");
					xdr_destroy(&xdrs_w);
					xdr_destroy(&xdrs_r);
					continue;
				}
				filesize = (uint32_t) filestat.st_size;
				timestamp = (uint32_t) filestat.st_mtime;
				printf("File size: %"PRIu32"\nLast mod: %"PRIu32"\n", filesize, timestamp);
				tag = OK;
				if (!xdr_tagtype (&xdrs_w, &tag)){
					mess_out.tag=ERR;
					xdr_message(&xdrs_w, &mess_out);
					sendn(connfd, buf, xdr_getpos(&xdrs_w), MSG_NOSIGNAL);
				 	printf("Error converting tagtype\n");
				 	xdr_destroy(&xdrs_w);
					xdr_destroy(&xdrs_r);
					continue;
				}
				if (!xdr_u_int (&xdrs_w, &filesize)){
					mess_out.tag=ERR;
					xdr_message(&xdrs_w, &mess_out);
					sendn(connfd, buf, xdr_getpos(&xdrs_w), MSG_NOSIGNAL);
	 			 	printf("Error converting filesize\n");
					xdr_destroy(&xdrs_w);
					xdr_destroy(&xdrs_r);
					continue;
				}
				if(sendn(connfd, buf, xdr_getpos(&xdrs_w), MSG_NOSIGNAL) != xdr_getpos(&xdrs_w)){
					mess_out.tag=ERR;
					xdr_message(&xdrs_w, &mess_out);
					sendn(connfd, buf, xdr_getpos(&xdrs_w), MSG_NOSIGNAL);
				 	printf("Error sending filesize\n");
				 	xdr_destroy(&xdrs_w);
					xdr_destroy(&xdrs_r);
					continue;
				}
						
				xdr_destroy(&xdrs_r);
				xdr_destroy(&xdrs_w);
				while( (nread = read(file, buf, BLKSIZE)) != -1 && nread == BLKSIZE) {
					if(sendn(connfd, buf, BLKSIZE, MSG_NOSIGNAL) != BLKSIZE){
						printf("Error sending data\n");
						break;
					}
				}
				
				if(nread < BLKSIZE){			
					//usiamo la memset per svuotare il buffer e riempirlo tutto di 0
					memset(&buf[nread], 0, sizeof(buf)-nread);
					resto=nread % 4;
					if(resto==0){
						if(sendn(connfd, buf, nread, MSG_NOSIGNAL) !=nread) { // se resto zero mando tutto il pacchetto da 4 bytes (nread multiplo di 4 ma minore di 1024)
							printf("Error sending last block of the file\n");
							break;
						}							
					}
					else {
						if(sendn(connfd, buf, (nread+resto), MSG_NOSIGNAL) != (nread+resto)){ //scriviamo nello stream nread caratteri più resto per ottenere un pacchetto multiplo di quattro bytes
							printf("Error sending last block with padding\n");
							break;
						}
					}
				}
				else{ //bytes_read == -1
					printf("(%s) error - read failed", prog_name);
				}
				
				/*Recreate the xdr encode buffer in order to send the timestamp*/
				xdrmem_create(&xdrs_w, buf, BLKSIZE, XDR_ENCODE);
				if (!xdr_u_int (&xdrs_w, &timestamp)){
					mess_out.tag=ERR;
					xdr_message(&xdrs_w, &mess_out);
					sendn(connfd, buf, xdr_getpos(&xdrs_w), MSG_NOSIGNAL);
	 			 	printf("Error converting timestamp\n");
					xdr_destroy(&xdrs_w);
					xdr_destroy(&xdrs_r);
					continue;
				}
				if(sendn(connfd, buf, xdr_getpos(&xdrs_w), MSG_NOSIGNAL) != xdr_getpos(&xdrs_w)){
					mess_out.tag=ERR;
					xdr_message(&xdrs_w, &mess_out);
					sendn(connfd, buf, xdr_getpos(&xdrs_w), MSG_NOSIGNAL);
				 	printf("Error sending timestamp\n");
				 	xdr_destroy(&xdrs_w);
					xdr_destroy(&xdrs_r);
					continue;
				}
				xdr_destroy(&xdrs_w);
				
				//per mandare il file dobbiamo emulare il protocollo xdr: non possiamo usare la xdr_message ma 
				//utilizzeremo la xdr_memcreate in cui andiamo a creare un campo tag da 8B, poi un campo per la lunghezza,
				//iniziare a leggere il file 4b alla volta e mandarli con una send avendo cura di mandare
				//secondo il protocollo cioè anche l'ultimo pacchetto deve essere da 4B (nel caso riempirlo con del padding)
				//infine mandare 8B con il timestamp
			}
			else if(mess_in.tag==QUIT){
				//Quit
				printf("(%s) --- connection correctly closed by client\n", prog_name);
				xdr_destroy(&xdrs_w);
				xdr_destroy(&xdrs_r);
				return(0);
			}
		}
	}
	return(0);
}

int transmitdata(int connfd){

	int nread, nsend, file;
	uint32_t filesize, converted_filesize;
	uint32_t timestamp, converted_timestamp;
	char filename[MAXBUFL+1], command[10+1];
	char buf[MAXBUFL+1];
	struct stat filestat;

	while(1){
		nread = readline_unbuffered (connfd, buf, MAXBUFL);
		if (nread <= 0){
			printf("(%s) --- connection closed by client or failed\n", prog_name);
			return(0);
		}
		if (nread == 0){
			continue; // torna al while ad aspettare;
		}
		else{
			// append the string terminator after CR-LF that is, \r\n (0x0d,0x0a)
			buf[nread]='\0';
			printf("(%s) --- received string: %s\n",prog_name, buf);
		
			// first validation of received string
			if(strncmp(buf, "GET ", 4) != 0 && strncmp(buf, "QUIT", 4) != 0){
				printf("(%s) --- error, riceived wrong command\n", prog_name);
				nsend = sendn(connfd, ERROR, strlen(ERROR), MSG_NOSIGNAL);
				//return(0);
			} else {
			
				if(strncmp(buf, "QUIT", 4) == 0){ // QUIT CASE
					printf("(%s) --- connection correctly closed by client\n", prog_name);
					return(0);
				} else { // GET CASE
					sscanf(buf, "%s %s\r\n", command, filename);
					printf("(%s) --- caso GET, riceived: %s %s\n", prog_name, command, filename);
				
					// opening the file
					if((file = open(filename, O_RDONLY)) == -1){
						nsend = sendn(connfd, ERROR, strlen(ERROR), MSG_NOSIGNAL);					
						printf("(%s) --- error opening the file\n", prog_name);
						return(0); 
					}
					if(stat(filename, &filestat) == -1){
						nsend = sendn(connfd, ERROR, strlen(ERROR), MSG_NOSIGNAL);					
						printf("(%s) --- error opening the file\n", prog_name);
						return(0); 
					}
					filesize = (uint32_t) filestat.st_size;
					timestamp = (uint32_t) filestat.st_mtime;
					printf("File size: %"PRIu32"\nLast mod: %"PRIu32"\n", filesize, timestamp);

					snprintf(buf, MAXBUFL, "+OK\r\n");
					nsend = sendn(connfd, buf, strlen(buf), MSG_NOSIGNAL);
					if(nsend != strlen(buf)){
						printf("(%s) --- cannot send +OK\n", prog_name);
						return(0);
					}
					// NB!!! to pass an integer in the network byte order I have to convert it and then to pass its pointer - NO STRING CONVERSION
					converted_filesize = htonl(filesize);
					nsend = sendn(connfd, &converted_filesize, sizeof(converted_filesize), MSG_NOSIGNAL);
					printf("(%s) --- sent converted size in network order\n", prog_name);
					// NB!!! to pass an integer in the network byte order I have to convert it and then to pass its pointer
					converted_timestamp = htonl(timestamp);
					nsend = sendn(connfd, &converted_timestamp, sizeof(converted_timestamp), MSG_NOSIGNAL);
					printf("(%s) --- sent converted timestamp in network order\n", prog_name);

					while((nread = read(file, buf, MAXBUFL)) != -1 && nread == MAXBUFL){
						if(sendn(connfd, buf, MAXBUFL, MSG_NOSIGNAL)!=MAXBUFL){
							printf("Error sending data\n");
							break;
						}
					}

					if(nread < MAXBUFL){			
						//empty the buffer from bytes_read to the end in order to avoid sending too many data, it causes problem in case of more requests
						memset(&buf[nread], 0, sizeof(buf)-nread);
				
						if(sendn (connfd, buf, nread, MSG_NOSIGNAL)!=nread){ // the last portion I send is the number of remaining bytes to send which is less than MAXBUFL
							printf("Error sending data\n");
						}
						printf("(%s) --- sent file %s\n", prog_name, filename);
					}
					else{ //bytes_read == -1
						printf("(%s) error - read failed", prog_name);
					}
					close(file);
				}
			}
		}
	}

	return(0);
}


int main (int argc, char *argv[]){

	int listenfd, connfd, i, father=0, xdr_flag=0;
	short port;
	struct sockaddr_in servaddr, cliaddr;
	socklen_t cliaddrlen = sizeof(cliaddr);

	// for errlib to know the program name 
	prog_name = argv[0];

	// check arguments 
	if (argc==3 || argc==4){		
		if(argc==4){
			if(strcmp(argv[1], "-x")==0){
				xdr_flag=1;
				port=atoi(argv[2]);
				numServ=atoi(argv[3]);
				if(numServ>10){
					printf("It is not possibile to create more than 10 servers! Try to create 10 servers\n");
					numServ=10;
				}
			}
			else{
				printf("usage: %s <-x> (optional) <port> <#ofServer>\n", prog_name);
				return(-1);
			}
		}
		else{
			xdr_flag=0;
			port=atoi(argv[1]);
			numServ=atoi(argv[2]);
			if(numServ>10){
				printf("It is not possibile to create more than 10 servers! Try to create 10 servers\n");
				numServ=10;
			}
		}
		
	}
	else{
		printf("usage: %s <-x> (optional) <port> <#ofServer>\n", prog_name);
		return(-1);
	}
	
	(void) signal(SIGINT, signalHandler);
	
	// create socket
	listenfd = Socket(AF_INET, SOCK_STREAM, 0);

	// specify address to bind to 
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	Bind(listenfd, (SA*) &servaddr, sizeof(servaddr));

	printf("(%s) socket created\n",prog_name);
	printf("(%s) listening on %s:%u\n", prog_name, inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));

	Listen(listenfd, LISTENQ);
		
	for(i=0; i<numServ; i++){
		pid[i]=fork();
		if(pid[i]!=0){
			//Father
			//printf("Creato figlio %d da %d\n", pid[i], getppid());
			father=1;
		}
		else{
			//child
			while (1) {
			fprintf(stderr, "(%s) waiting for connections ...\n", prog_name); fflush(stderr);

			int retry = 0;
			do {
				connfd = accept (listenfd, (SA*) &cliaddr, &cliaddrlen);
				if (connfd<0) {
					if (INTERRUPTED_BY_SIGNAL ||
						errno == EPROTO || errno == ECONNABORTED ||
						errno == EMFILE || errno == ENFILE ||
						errno == ENOBUFS || errno == ENOMEM	) {
						retry = 1;
						err_ret ("(%s) error - accept() failed", prog_name);
					} else {
						err_ret ("(%s) error - accept() failed", prog_name);
						return 1;
					}
				} else {
					fprintf(stdout, "(%s) - new connection from client %s:%u\n", prog_name, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port)); fflush(stdout);
					retry = 0;
				}
			} while (retry);
			
			if(xdr_flag==0){
				transmitdata(connfd);
			}else transmitdata_xdr(connfd);
			
			Close (connfd);
			}
			break;
		}
	}
	
	if(father==1){
		//printf("Padre \n");
		//Father 
		for(i=0; i<numServ; i++){
			//printf("Wait numero: %d\n", i);
			wait(NULL);
		}
	}
	
	return 0;
}

