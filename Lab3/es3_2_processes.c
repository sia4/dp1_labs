#include     <stdio.h>
#include     <stdlib.h>
#include     <unistd.h>
#include     <string.h>
#include     <inttypes.h>
#include 	 <sys/socket.h>
#include 	 <signal.h>

#include     "../libraries/errlib.h"
#include     "../libraries/sockwrap.h"

#define BUFLEN 128
char *prog_name;

void client_routine(int s, char *filename);
int read_file(char *filename, uint32_t n_read, int s, char *rbuf);

/** ADDED **/
typedef struct pl *pid_list_link;
typedef struct pl{
	pid_t pid;
	pid_list_link next;
} pid_list;

// A SIGPIPE is sent to a process if it tried to write to a socket that had been shutdown for writing or isn't connected (anymore).
void sigpipe_handler(int signal) {
  
	kill(getpid(), SIGKILL);
	return;
}

int main(int argc, char *argv[]) {

  	struct addrinfo *res, hints, *p;
    int	s;

    /* exercise params */
    char buf[BUFLEN];
    char filename[BUFLEN];			/* transmission buffer */
    char rbuf[BUFLEN];			/* reception buffer */
	int end;

	/** ADDED **/
	pid_list *head = NULL, *tail = NULL;
	pid_t cpid;
	pid_list *supp;
	int found;

	prog_name = argv[0];
	
	if(argc != 3)
		err_quit ("usage: %s <address> <port>\n", prog_name);

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

	/* main client loop */
	end = 0;
    while(!end) {

    	memset(buf, 0, BUFLEN);
    	memset(rbuf, 0, BUFLEN);

		printf("Menu options:\n- GET <filename>: perform the get of a file from the server;\n\
		- Q: Close the connection after file transfer;\n \
		- A: Forcefully close the connection.\n");
		
		/* read from stdin */
		fgets(buf, BUFLEN, stdin);
		switch(buf[0]) {
		case 'G':
		case 'g':
			strcpy(filename, &buf[4]);
			break;
		case 'Q':
		case 'q':
			strcpy(filename, "QUIT");
			printf("Client ends, the download will continue...\n"); //non sicura sia giusto
			end = 1;
			break;
		case 'A':
		case 'a':

			supp = head;	
			while(supp != NULL) {
				kill(supp->pid, SIGKILL);
				supp = supp->next;
			}
			Close (s);   // This will give "connection reset by peer at the server side
            err_quit("(%s) - exiting immediately", prog_name);
			break;		
		default: 
			printf("Unknown command: %s\n", buf);
			continue;
		}
    	cleanString(filename);
	
		cpid = fork();	
		if(!cpid) {
			client_routine(s, filename);

			supp = head;
			found = 0;
			if(supp->pid == getpid()) {
				head = head->next;
				found = 1;
			}
				
			while(supp->next != NULL && !found) {
				if(supp->next->pid == getpid()) {
					supp->next = supp->next->next;
					found = 1;
				}
				supp = supp->next;
			}

			return 0;
		} else {
			
			if(tail == NULL) {
				tail = malloc(sizeof(pid_list));
				tail->pid = cpid;
				tail->next = NULL;
				head = tail;
			} else {
				tail->next = malloc(sizeof(pid_list));
				tail->next->pid = cpid;
				tail = tail->next;
				tail->next = NULL;
			}

		}

    	}
    

	Close(s);
	printf("\nQUIT received, socket closed!\n");
	/* close everything and terminate */
	exit(0);    	
}

void client_routine(int s, char *filename) {

	size_t len;
	int result;
	uint32_t filesize, timestamp;
	char rbuf[BUFLEN];
	char buf[BUFLEN];

	if((strncmp(filename, "QUIT", 4)) == 0 || (strncmp(filename, "quit", 4) == 0)) {
		/* send quit message */
		if(writen(s, "QUIT\r\n", 6) != 6)
		    err_msg("(%s) ERROR! Server Unreacheable!\n", prog_name);
	} else {

    	/* send params to server */
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