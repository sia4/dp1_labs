#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <inttypes.h>
#include <fcntl.h>

#include <string.h>
#include <time.h>

#include <rpc/xdr.h>

#include    "../../libraries/errlib.h"
#include	"../../libraries/sockwrap.h"
#include    "types.h"

#define BUFLEN		 128
#define LEN 		 32
#define BKLOG		 3
#define MAX_CHILDREN 10
#define TIMEOUT		 120
#define BLKSIZE      1024

char *prog_name;
char *get_filename(char *buf);

int performClientRequest(int socket, int i);
int performClientRequestXDR(int s1, int i);

int main(int argc, char **argv){

	/* var for connection */
	int s, s1, bklog = 2;
	struct sockaddr_storage caddr;
	struct addrinfo *res, hints, *p;
	socklen_t caddr_len;
	int n_children, i;
	char *port;


	int xdr_flag = 0;

	prog_name = argv[0];
	caddr_len = sizeof(struct sockaddr_storage);

	if(argc < 3)
		err_quit("Wrong input format!\nThe right format should be: %s [<-x>] <source_port> <n_children>\n", argv[0]);
	if(argc == 4 && argv[1][1] == 'x') {
		xdr_flag = 1;
		port = argv[2];
		n_children = atoi(argv[3]);
	} else {
		port = argv[1];
		n_children = atoi(argv[2]);
	}
	
	if(n_children > MAX_CHILDREN)
		err_quit("Number of children must be lower than %d.\n", MAX_CHILDREN);

	/* use getaddrinfo to parse address and port number */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	Getaddrinfo(NULL, port, &hints, &res);
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
    printf("Bind done.\n");

	/* listen */
    printf ("Listening at socket %d with backlog = %d \n",s,bklog);
    Listen(s, bklog);
    printf("Listen done.\n");

	freeaddrinfo(res); // all done with this structure

	for(i = 0; i < n_children; i++){

		if(fork() == 0) {

			/*Server loop*/
			for(;;){
				printf("(%s - %d) - Waiting for connection...\n", prog_name, i);
		
				/*Accept*/
				s1 = Accept(s, (struct sockaddr *) &caddr, &caddr_len); 
				showAddr("Accepted connection from", (struct sockaddr_in*) &caddr);

				if(xdr_flag) {
					if(performClientRequestXDR(s1, i))
						printf("(%s - %d) - Connection closed by the client!\n", prog_name, i);
					else
						printf("(%s - %d) - Timeout expired!\n", prog_name, i);
				} else {
					printf("debug - sono qui"); 
					if(performClientRequest(s1, i))
						printf("(%s - %d) - Connection closed by the client!\n", prog_name, i);
					else
						printf("(%s - %d) - Timeout expired!\n", prog_name, i);
				}
				Close(s1);
			}		
			return 0;
		} 
	}

	for(i = 0; i < n_children; i++)
		wait(NULL);

	Close(s);
	return(EXIT_SUCCESS);
}

int performClientRequestXDR(int s1, int i) {

	/* prog var */
	int file;
	uint32_t filesize;
	uint32_t timestamp;
	char buf[BLKSIZE+1];
	struct stat filestat;
	//int nread, resto;

	/* xdr var */
	message mess_in, mess_out;
	XDR xdrs_w, xdrs_r;
	FILE *fstream_r, *fstream_w;
	//tagtype tag;

	/* create file fstream_w */
	fstream_w = fdopen(s1, "w");
	if (fstream_w == NULL){
		fprintf(stdout, "(%s) error - fdopen() fstream_r failed\n", prog_name); fflush(stdout);
		return(-1);
	}

	/* creating XDR stream */
	xdrstdio_create(&xdrs_w, fstream_w, XDR_ENCODE); 

	/* create file fstream_r */
	fstream_r = fdopen(s1, "r");
	if (fstream_r == NULL){
		fprintf(stdout, "(%s) error - fdopen() fstream_r failed\n", prog_name); fflush(stdout);
		return(-1);
	}

	/* creating XDR stream */
	xdrstdio_create(&xdrs_r, fstream_r, XDR_DECODE); 

	memset (&mess_in,0,sizeof(message));
	memset (&mess_out,0,sizeof(message));	

	while(1){
	
		printf("Waiting for commands...\n");
		
		/* reading command by the client */
		if (xdr_message(&xdrs_r, &mess_in)==0){
			printf("(%s) --- cannot read the message sent by the client\n", prog_name);
			xdr_destroy(&xdrs_w);
			xdr_destroy(&xdrs_r);
			return(-1);
		}
		fprintf(stdout, "(%s) --- received string: %s\n",prog_name, mess_in.message_u.filename); fflush(stdout);
		if(mess_in.tag==GET){
			file = open( mess_in.message_u.filename, O_RDONLY);
			if(file == -1){
				mess_out.tag=ERR;
				xdr_message(&xdrs_w, &mess_out);
				sendn(s1, buf, xdr_getpos(&xdrs_w), MSG_NOSIGNAL);
				printf("Cannot open the file\n");
				xdr_destroy(&xdrs_w);
				xdr_destroy(&xdrs_r);
				continue;	
			}
			if(stat(mess_in.message_u.filename, &filestat) == -1){
				mess_out.tag=ERR;
				xdr_message(&xdrs_w, &mess_out);
				sendn(s1, buf, xdr_getpos(&xdrs_w), MSG_NOSIGNAL);
				printf("Cannot stat the file\n");
				xdr_destroy(&xdrs_w);
				xdr_destroy(&xdrs_r);
				continue;
			}

			filesize = (uint32_t) filestat.st_size;
			timestamp = (uint32_t) filestat.st_mtime;
			printf("File size: %"PRIu32"\nLast mod: %"PRIu32"\n", filesize, timestamp);
			mess_out.tag = OK;
			mess_out.message_u.fdata.contents.contents_len = filesize;
			mess_out.message_u.fdata.last_mod_time = timestamp;
			mess_out.message_u.fdata.contents.contents_val = malloc((filesize+1)*sizeof(char));
			read(file, mess_out.message_u.fdata.contents.contents_val, filesize);     //fread(mess_out.message_u.fdata.contents.contents_val, filesize, 1, file);
			mess_out.message_u.fdata.contents.contents_val[filesize+1] = '\0';

			if (!xdr_message (&xdrs_w, &mess_out)){

				memset (&mess_out,0,sizeof(message));	
				mess_out.tag=ERR;
				xdr_message(&xdrs_w, &mess_out);
			 	printf("Error converting tagtype\n");
			 	xdr_destroy(&xdrs_w);
				xdr_destroy(&xdrs_r);
				continue;

			}
			fflush(fstream_w);

			printf("Message sent.\n");
			free(mess_out.message_u.fdata.contents.contents_val);
		}
		else if(mess_in.tag==QUIT){
			//Quit
			printf("(%s) --- QUIT command! connection correctly closed by client\n", prog_name);
			xdr_destroy(&xdrs_w);
			xdr_destroy(&xdrs_r);
			return(0);
		}


	}

	xdr_destroy(&xdrs_w);
	xdr_destroy(&xdrs_r);
	fclose(fstream_r);
	return(0);

}

int performClientRequest(int s1, int i) {

	/* program vars */
	char *filename;
	struct stat info;
	int res;
	int fd;
	uint32_t dim, networkFileSize, networkTime;
	char filebuf[BUFLEN];
	int nread;
	int n;
	char buf[BUFLEN];

	struct timeval tval;
	fd_set cset;

	printf("\n(%s %d) - Waiting a command...\n", prog_name, i);	

    FD_ZERO(&cset); //Clears the set
    FD_SET(s1, &cset); //Add a file descriptor to a set
    tval.tv_sec = TIMEOUT; //Timeout interval
    tval.tv_usec = 0;

	while(Select(FD_SETSIZE, &cset, NULL, NULL, &tval)) { //Waits until the file descriptor is ready
	
		n = Readline_unbuffered(s1, buf, BUFLEN);
		if(n == 0)
			return 1;

		cleanString(buf);
		printf("(%s %d) - Received: %s\n", prog_name, i, buf);
		if(!strncmp(buf, "QUIT", 4)) {
			return 0;
		} else if(strncmp(buf, "GET ", 4)){
			printf("(%s %d) - Wrong request!\n", prog_name, i);
			Send(s1, "-ERR\r\n", 6*sizeof(char), 0);
			continue;
		}
		filename = get_filename(buf);

		//printf("%s\n", filename);
		res = stat(filename, &info);
		if(res == -1) {
			Send(s1, "-ERR\r\n", 6*sizeof(char), 0);
			printf("(%s %d) - The file doesn't exist on the server!\n", prog_name, i);
		} else {
			fd = open(filename, O_RDONLY);
			if(fd == -1){
				Send(s1, "-ERR\r\n", 6*sizeof(char), 0);
				printf("(%s %d) - Error in opening file.\n", prog_name, i);
				continue;	
			}else{
				Send(s1, "+OK\r\n", 5*sizeof(char), 0);			 

	 			dim = (uint32_t) info.st_size;
				networkFileSize = htonl(dim);
				Send(s1, &networkFileSize, sizeof(uint32_t), 0);

				networkTime = htonl(info.st_mtime);
				Send(s1, &networkTime, sizeof(uint32_t), 0);

				memset(filebuf, 0, BUFLEN);
	 			while((nread = read(fd, filebuf, BUFLEN-1)) != -1 && nread != 0) {
	 				printf("%s", filebuf);
					Sendn(s1, filebuf, nread, 0);
					memset(filebuf, 0, BUFLEN);
				}
	 			close(fd);
				printf("(%s %d) - The file %s was sended!\n",prog_name, i, filename);
			}
		}
 
		printf("\n(%s - %d) - Waiting a command...\n", prog_name, i);					
	}	

	return 1;
}

char *get_filename(char *buf){
	char *name;
	name = malloc(BUFLEN*sizeof(char));
	int i;
	char c;
	
	for(i = 4; i < strlen(buf); i++){
		c = buf[i];
		if(c == '\r' || c == '\n'){
			name[i-4] = '\0';
			break;
		}
		name[i-4] = c;
	}
	
	return name;
}