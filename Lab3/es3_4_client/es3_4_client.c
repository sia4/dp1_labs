#include     <stdio.h>
#include     <stdlib.h>
#include     <unistd.h>
#include     <string.h>
#include     <inttypes.h>
#include 	 <sys/socket.h>
#include 	 <signal.h>
#include     <fcntl.h>
#include     <rpc/xdr.h>

#include	 <errno.h>
#include 	 "../../libraries/sockwrap.h"
#include 	 "../../libraries/errlib.h"
#include     "types.h"

/*
 *	struct in_addr { unsigned long s_addr; };
 *
 *	struct sockaddr_in {
 *		short            sin_family;   // e.g. AF_INET
 *		unsigned short   sin_port;     // e.g. htons(3490)
 *		struct in_addr   sin_addr;     // the previous one
 *		char             sin_zero[8];  // zero this if you want to
 *	};
 */

#define ADDR_LEN 14 
#define BUFLEN	128
#define LEN 32

/* The type used for socket identification */
typedef int SOCKET;
char *prog_name;
int end;

/* FUNCTION PROTOTYPES */
void client_routine_XDR(char *filename, int s);
void client_routine(int s, char *filename);
int read_file(char *filename, uint32_t n_read, int s, char *rbuf);

int main(int argc, char **argv){

	/* var for connection */
	int s;
	struct addrinfo *res, hints, *p;
	char *address, *port;

	/** ADDED **/
	char input[LEN];
	int xdr_flag = 0;

	if(argc < 3)
		err_quit ("usage: <program name> [<-x>] <address> <port>\n");
	if(argc == 4 && argv[1][1] == 'x') {
		xdr_flag = 1;
		address = argv[2];
		port = argv[3];
	} else {
		address = argv[1];
		port = argv[2];
	}

	prog_name = argv[0];

	/* use getaddrinfo to parse address and port number */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
	hints.ai_socktype = SOCK_STREAM;
	Getaddrinfo(address, port, &hints, &res);
	printf("Address and port parsed with getaddrinfo.\n");

	/* loop through all the results and connect to the first we can */
	for(p = res; p != NULL; p = p->ai_next) {
	    if ((s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
	        continue;
	    }

	    printf("Socket created. Socket number: %d\n",s);

	    /* Connection */
	    if (connect(s, p->ai_addr, p->ai_addrlen) == -1) {
	        Close(s);
	        continue;
	    }

	    printf("Successfully connected.\n");

	    break; // if we get here, we must have connected successfully
	}
    
	if (p == NULL) {
	    // looped off the end of the list with no successful bind
	    err_msg("(%s) ERROR! Failed to bind socket!\n", prog_name);
	    exit(2);
	}

	freeaddrinfo(res); // all done with this structure
	
	/* client loop */
	end = 0;
	while(!end) {

		printf("Menu options:\n- GET <filename>: perform the get of a file from the server;\n\
- QUIT: Close the connection.\n");

		memset(input, 0, LEN);
		fgets(input, LEN-1,stdin);

		cleanString(input);
		
		if(xdr_flag)
			client_routine_XDR(input, s);
		else
			client_routine(s, input);

	}

	Close(s);
    printf("\nSocket closed!\n");
    return(EXIT_SUCCESS);
}

void client_routine_XDR(char *input, int s) {

	int fd;
	uint32_t  filesize, timestamp;
	char filename[LEN];

	message mess_in, mess_out; 
	XDR xdrs_w, xdrs_r;
	FILE *fstream_w, *fstream_r;

	/* create file fstream_w */
	fstream_w = fdopen(s, "w");
	if (fstream_w == NULL){
		fprintf(stdout, "(%s) error - fdopen() fstream_w failed\n", prog_name); fflush(stdout);
		return;
	}
	/* creating XDR stream */
	xdrstdio_create(&xdrs_w, fstream_w, XDR_ENCODE);
	
	/* create file fstream_r */
	fstream_r = fdopen(s, "r");
	if (fstream_r == NULL){
		fprintf(stdout, "(%s) error - fdopen() fstream_r failed\n", prog_name); fflush(stdout);
		xdr_destroy(&xdrs_w);
		return;
	}
	/* creating XDR stream */
	xdrstdio_create(&xdrs_r, fstream_r, XDR_DECODE);

	mess_out.message_u.filename = malloc(BUFLEN*sizeof(char));
	mess_in.message_u.filename = malloc(BUFLEN*sizeof(char));
	//mess_in.message_u.fdata.contents.contents_val = malloc(BUFLEN*sizeof(char));
	
	memset (&mess_in,0,sizeof(message));
	memset (&mess_out,0,sizeof(message));

	if((strcmp(input, "QUIT") == 0) || (strcmp(input, "quit") == 0)){
		mess_out.tag = QUIT;
		//printf("message: %d\n", mess_out.tag);
		if(xdr_message(&xdrs_w, &mess_out) == 0) {
			printf("(%s) - Error closing connection.\n", prog_name);
			xdr_destroy(&xdrs_w);
			xdr_destroy(&xdrs_r);
			end = 1;
			return;
		} else {
			printf("(%s) - Connection closed.\n", prog_name);
			xdr_destroy(&xdrs_w);
			xdr_destroy(&xdrs_r);
			end = 1;
			return;
		}

		fflush(fstream_w);

	} else if(strncmp(input, "GET", 3) == 0 || strncmp(input, "get", 3) == 0) {
		strcpy(filename, &input[4]);
		mess_out.tag = GET;
		mess_out.message_u.filename=filename;

		/* sending the buffer with get tag and filename */
	   if(xdr_message(&xdrs_w, &mess_out) == 0){
			printf("(%s) Error sending file name\n", prog_name);
			xdr_destroy(&xdrs_w);
			xdr_destroy(&xdrs_r);
			return;
		}
		fflush(fstream_w);

		printf("Message sent: %d %s\n", mess_out.tag, mess_out.message_u.filename);
		/* receving the message from server */
		int res = xdr_message(&xdrs_r, &mess_in);
		printf("res = %d\n", res);
		if(res==0){
			printf("(%s) Error receiving data\n", prog_name);
			xdr_destroy(&xdrs_w);
			xdr_destroy(&xdrs_r);
			end = 1;
			return;
		} else if(mess_in.tag == ERR){ // ERR case
			printf("(%s) Error received from server\n", prog_name);
			xdr_destroy(&xdrs_w);
			xdr_destroy(&xdrs_r);
			return;
		} else if(mess_in.tag == OK){ // OK case
	
			printf("(%s) Ok received from the server\n", prog_name);
			filesize = mess_in.message_u.fdata.contents.contents_len; //do not use ntohl, xdr do it in an automatic way 
			timestamp = mess_in.message_u.fdata.last_mod_time;
			printf("(%s) Received filesize and timestamp: %d, %d\n", prog_name, filesize, timestamp);

			/*opening the file */
			if((fd = open(filename, O_RDWR | O_CREAT, 0777)) == -1){ // O_CREAT creates the file if it does not exist
				printf("(%s) Error creating the file\n", prog_name);
				xdr_destroy(&xdrs_w);
				xdr_destroy(&xdrs_r);
				return;
			}

			printf("Write file\n");
			/* writing the file */
			printf("File: %s\n",  mess_in.message_u.fdata.contents.contents_val);
			write(fd, mess_in.message_u.fdata.contents.contents_val, mess_in.message_u.fdata.contents.contents_len);
		    close(fd);
			printf("(%s) File received!\n", prog_name);	
			xdr_destroy(&xdrs_w);
			xdr_destroy(&xdrs_r);
			return;
		}
		printf("End of cycle\n");
	} else {
		printf("(%s) - Unknown command: %s\n", prog_name, input);
	}

	return;
}

void client_routine(int s, char *input) {

	size_t len;
	int result;
	uint32_t filesize, timestamp;
	char rbuf[BUFLEN];
	char buf[BUFLEN];
	char filename[BUFLEN];

	if((strncmp(input, "QUIT", 4)) == 0 || (strncmp(input, "quit", 4) == 0)) {
		/* send quit message */
		if(writen(s, "QUIT\r\n", 6) != 6)
		    err_msg("(%s) ERROR! Server Unreacheable!\n", prog_name);
	} else if(strncmp(input, "GET", 3) == 0 || strncmp(input, "get", 3) == 0) {

    	/* send params to server */
    	sscanf(input, "%*s %s", filename);
    	sprintf(buf, "GET %s\r\n", filename);
    	len = strlen(buf);
		if(writen(s, buf, len) != len) {
		    err_msg("(%s) ERROR! Server Unreacheable!\n", prog_name);
		    return;
		}

		/* read from server */
		printf("Waiting for response...\n");
		result = readline_unbuffered(s, rbuf, BUFLEN);
		if (result <= 0) {
		    err_msg("(%s) ERROR! Server Unreacheable!\n", prog_name);
		    return;
		}

		if(strncmp(rbuf, "+OK\r\n", 5) == 0) {
			result = readn(s, &filesize, sizeof(uint32_t));
			if (result <= 0) {
			    err_msg("(%s) ERROR! Server Unreacheable!\n", prog_name);
		    	return;
			}
			result = readn(s, &timestamp, sizeof(uint32_t));
			if (result <= 0) {
			    err_msg("(%s) ERROR! Server Unreacheable!\n", prog_name);
		    	return;
			}

			filesize = ntohl(filesize);
			timestamp = ntohl(timestamp);


			if(read_file(filename, filesize, s, rbuf) == -1)
				return;

			printf("File transfer completed.\n");
			printf("Filename = %s;\nSize = " "%" SCNu32 ";\nLast modification timestamp = " "%" SCNu32 ".\n", filename, filesize, timestamp);
			
		} else if(strncmp(rbuf, "-ERR\r\n", 6) == 0) {
			printf("(%s) ERROR! The file probably doesn't exist!\n\n --- Usage ---\n", prog_name);
            printf("<filename>: allows to you to download a file \nQUIT: close the program.\n\n");
		} else {
			printf("(%s) ERROR! The file probably doesn't exist!\n\n --- Usage ---\n", prog_name);
            printf("<filename>: allows to you to download a file \nQUIT: close the program.\n\n");
		}		
	} else {
		printf("(%s) ERROR! Wrong command!\n\n --- Usage ---\n", prog_name);
        printf("<filename>: allows to you to download a file \nQUIT: close the program.\n\n");
	}
    

}

int read_file(char *filename, uint32_t n_read, int s, char *rbuf) {

	int result;
	FILE *fp;

	fp = fopen(filename, "w");
	if(fp == NULL) {
		err_msg("(%s) ERROR! Can't open file!\n", prog_name);
		return -1;
	}
	printf("File created.\n");

	/* read and store file */
	while(n_read > 0) {
		if(BUFLEN < n_read) {
			result = readn(s, rbuf, BUFLEN-1);
			if (result <= 0) {
				fclose(fp);
			    err_msg("(%s) ERROR! A Server Unreacheable!\n", prog_name);
			    return -1;
			}
			n_read -= BUFLEN-1;
		} else {
			memset(rbuf, 0, BUFLEN);
			result = readn(s, rbuf, n_read);
			if (result <= 0) {
				fclose(fp);
				err_msg("(%s) ERROR! B Server Unreacheable!\n", prog_name);
			    return -1;
			}
			n_read = 0;
		}
		fprintf(fp, "%s", rbuf);
	}

	fclose(fp);

	return 0;
}