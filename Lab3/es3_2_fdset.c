#include     <stdio.h>
#include     <stdlib.h>
#include     <unistd.h>
#include     <string.h>
#include     <inttypes.h>
#include 	 <sys/socket.h>
#include 	 <signal.h>

#include	 <errno.h>
#include 	 "../libraries/sockwrap.h"
#include 	 "../libraries/errlib.h"

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

#define BUFLEN	128
#define LEN 32

char *prog_name;

/* FUNCTION PROTOTYPES */
int check_digit(char c);
void clientRoutine(char *filename, int s);

/** ADDED **/
typedef struct pl *pid_list_link;
typedef struct pl{
	pid_t pid;
	pid_list_link next;
} pid_list;

int main(int argc, char **argv){

	/* var for connection */
	struct addrinfo *res, hints, *p;
    int	s;

	int end;	

	/* prog vars */
	char filename[LEN];
	char answer[LEN];
	uint32_t filesize, time;
 	FILE *fp;

	/** ADDED **/
	char input[LEN];
	int reading_file = 0;
	fd_set rset;
	int file_ptr;
	char c;

	if(argc != 3)
		err_quit ("usage: <program name> <address> <port>\n");

	prog_name = argv[0];
	signal(SIGPIPE, sigpipe_handler);

	/* use getaddrinfo to parse address and port number */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
	hints.ai_socktype = SOCK_STREAM;
	Getaddrinfo(argv[1], argv[2], &hints, &res);
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
	printf("Menu options:\n- GET <filename>: perform the get of a file from the server;\n\
- Q: Close the connection after file transfer;\n\
- A: Forcefully close the connection.\n");

	while(1) {

		if(reading_file == 0 && end == 1)
			break;

		FD_ZERO(&rset);
		FD_SET(s, &rset);
		FD_SET(fileno(stdin), &rset);

		Select(s+1, &rset, NULL, NULL, NULL);
		// the select operation allows the program to monitor different file descriptor until one or more become "ready"
		// to perform an I/O operation without blocking
		//FD_SET is used to add a file desciptor to monitor
		//select first parameter is the highest file descriptor number + 1

		printf("qui 1s\n");
		if(FD_ISSET(s, &rset)) {
		//It is possible to read from socket
			printf("qui 2\n");
			if(reading_file == 0) {
				printf("qui 3\n");
			//Not still reading a file, receving first message from server (OK or ERROR message)
				if (readline_unbuffered(s, answer, LEN) <= 0) {
					Close(s);
		            err_quit("(%s) ERROR! Server Unreacheable!\n", prog_name);
				}else if(!strncmp(answer, "+OK", 3)) {
					printf("%s received!\n", answer);
		            if( !readn(s, &filesize, sizeof(uint32_t)) )
		                err_quit("(%s) ERROR! Server Unreacheable!\n", prog_name);

					filesize = ntohl(filesize);
		            printf("The size of the file is: %d\n", filesize);
				
					if( !readn(s, &time, sizeof(uint32_t)) )
		                err_quit("(%s) ERROR! Server Unreacheable!\n", prog_name);
					time = ntohl(time);
					printf("The time of last mod is: %d\n", time);

		            fp = fopen(filename, "w");
		            if(fp == NULL){
						Send(s, "QUIT\r\n", 7, 0);
						Close(s);
						err_quit("It's impossible to create the file\n!");
					}

					reading_file = 1;					
					file_ptr = 0;
					err_msg("(%s) --- opened file '%s' for writing",prog_name, filename);

				} else {
		            printf("(%s) ERROR! The file probably doesn't exist!\n\n", prog_name);
            	}
			} else if(reading_file == 1) {
                Read (s, &c, sizeof(char));
                fwrite(&c, sizeof(char), 1, fp);
                file_ptr++;

                if (file_ptr == filesize) {
                    fclose(fp);
                    err_msg("(%s) --- received and wrote file '%s'",prog_name, filename);
                    reading_file = 0;
                }

			} else {
				err_quit("(%s) - flag reading_file error '%d'", prog_name, reading_file);
			}

		} else if(FD_ISSET(fileno(stdin), &rset)) {
		//It is possible to read from stdin
			input[0] = '\0';
			fgets(input, LEN,stdin);
			cleanString(input);

			switch(input[0]) {
			case 'G':
			case 'g':
				printf("(%s) --- GET command sent to server\n",prog_name);
				strcat(input, "\r\n");
				Write(s, input, strlen(input));
				sscanf(input, "%*s %s", filename);
				//printf("filename: %s\n", filename); 
				break;
			case 'Q':
			case 'q':
				sprintf(input, "QUIT\r\n");
				Write(s, input, strlen(input));
				Shutdown(s, SHUT_WR); //cos'è???
				printf("Client ends, the download will continue...\n");
				end = 1;
				break;
			case 'A':
			case 'a':
				Close (s);   // This will give "connection reset by peer at the server side
		        err_quit("(%s) - exiting immediately", prog_name);
				break;		
			default: 
				printf("Unknown command: %s\n", input);
			}	

		}
	}
    return(EXIT_SUCCESS);
}


