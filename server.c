#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include "CommonCrypto/CommonCrypto.h"

#define BUFFERLENGTH 8192 //8MB 

void free_socket(int * socket){
    close(*socket);
    free(socket);
}

int checkIO(int res, char ** buffer){
    if(res==0){
        free(*buffer);
        printf("ERROR: Client ended connection!\n");
        return 1;
    }

    if(res < 0){
        free(*buffer);
        printf("ERROR: Could not read from socket!\n");
        return 1;
    }

    return 0;
}

int processFile(int * new_conn){
    int read_bytes = 0; //keep track of bytes read from client
    int recv_res; //updated for every read

    /* Changing socket descriptor to non-blocking */
    int flag; 
    if ((flag = fcntl(*new_conn, F_GETFL, 0)) < 0) {
        printf("Error setting socket to non-blocking\n");
        return 1;
    }
    if (fcntl(*new_conn, F_SETFL, flag | O_NONBLOCK) < 0) { 
        printf("Error setting socket to non-blocking\n");
        return 1;
    } 

    //Assign buffer to hold data from client
    char * buffer = malloc(sizeof(char) * BUFFERLENGTH); 
    if(!buffer){
        printf("Error allocating memory\n");
        return 1;
    }

    /* 

    Unfortunately the 'read' function kept hanging; changing the socket to non-blocking
    meant I could not read properly for some reason (kept returning -1). I used the 
    'select()' call below to see if there is data waiting on the socket ... unfortunately 
    this blocks too :( 

    I therefore added a timeout of a second (assumes no more data waiting) on the 
    select call; probably not the best method but it works.

                                                                                    */

    //Add connection to select pool
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(*new_conn, &fds);

    //set timer for select call
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 500000;

    //set common crypto credntials 
    CC_SHA256_CTX hash;
    CC_SHA256_Init(&hash);
    unsigned char digest[CC_SHA256_DIGEST_LENGTH]; 

    //read in data from client
    int currentBuffer = BUFFERLENGTH;
    while(1){
        if (select(*new_conn+1, &fds, 0, 0, &tv)==1){ //check if data waiting
            
            recv_res = read(*new_conn, buffer+read_bytes, currentBuffer - read_bytes);
            if(checkIO(recv_res, &buffer)!=0)
                return 1;

            read_bytes+=recv_res;
            CC_SHA256_Update(&hash, buffer, read_bytes); //update hash
        }else{
            break;
        }
    }
    
    //final digest
    CC_SHA256_Final(digest, &hash);

    int write_res = 0;
    int res;
    //Write hashed file back to client
    while(write_res!=CC_SHA256_DIGEST_LENGTH){
        res = write (*new_conn, digest, CC_SHA256_DIGEST_LENGTH);

        if(checkIO(res, &buffer)!=0)
                return 1;

        write_res+=res;
     }

    shutdown(*new_conn, SHUT_WR); //prevent any more sends (can receive)

    //New timer for response
    struct timeval tv2;
    tv2.tv_sec = 5;
    tv2.tv_usec = 500000;

    //Reset select 
    FD_ZERO(&fds);
    FD_SET(*new_conn, &fds);

    if (select(*new_conn+1, &fds, 0, 0, &tv2)==1){ //wait for response that file received
        char response[512];
        int res = read(*new_conn, response, 511);
        response[res]='\0';
        
        printf("%s\n", response);
    }else{
        printf("ERROR: Read timed out\n");
    }

    //Clean up
    free(buffer);
    free_socket(new_conn);

    return 0;
 }

void initiateServer(int portno){
	int socketfd; // return value for initiation
	struct sockaddr_in server_address;
	struct sockaddr_in client_address;
	socklen_t client_length = sizeof(client_address); // size of address of connecting program

	//socket creation
	socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if(socketfd < 0){
		printf("ERROR: Could not open socket\n");
		exit(1);
	}
	memset((char *)&server_address, '\0', sizeof(server_address));

	//set struct properties
	server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(portno);

    //bind socket to address
    int res = bind(socketfd, (struct sockaddr *) &server_address, sizeof(server_address));
    if(res < 0){
    	printf("ERROR: Could not bind socket\n");
		exit(1);
    }

    //listen (5 is number of allowed queued connections)
    listen(socketfd, 10);

    for(;;){
    	int * new_conn = malloc(sizeof(int)); 

    	//new connection
    	*new_conn = accept(socketfd, (struct sockaddr *) &client_address, &client_length);
    	if(*new_conn < 0 ){
    		printf("ERROR: Could not accept connection\n");
            free(new_conn);
    	}

        int pid = fork();
        if(pid<0){
            printf("ERROR: Could not fork\n");
            free_socket(new_conn);
            continue;
        }

       //Child process
       if (pid==0){
            int proc = processFile(new_conn);
            if (proc!=0) {
                printf("ERROR: Could not process file\n");
                free_socket(new_conn);
            }
            exit(0);
        }
    }
}

int main(int argc, char * argv[]){
	int portno;

	if(argc < 2){
		printf("Usage: %s <portno>\n", argv[0]);
		exit(0);
	}

	portno = atoi(argv[1]);

	initiateServer(portno);
	return 0;
}