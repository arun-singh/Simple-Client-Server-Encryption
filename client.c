#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "CommonCrypto/CommonCrypto.h"


#define BUFFERLENGTH 512

void raise_error(char *msg){
    printf("%s\n", msg);
    exit(0);
}

void checkIO(int res, char ** buffer, int socketfd){
    if(res==0){
        free(*buffer);
        close(socketfd);
        raise_error("ERROR: Server closed connection");
    }else if(res < 0){
        free(*buffer);
        close(socketfd);
        raise_error("ERROR: Could not write to socket");
    }
}


int main(int argc, char *argv[]){
    //Connection credentials
    int socketfd, res;
    struct sockaddr_in server_address;
    struct hostent *server;
    char * filename;

    if (argc < 4) {
       printf("Usage: %s <hostname> <portno> <filetoencrypt>\n", argv[0]);
       exit(1);
    }

    int portno = atoi(argv[2]);
    socketfd = socket (AF_INET, SOCK_STREAM, 0);
    if (socketfd < 0){
        raise_error("ERROR: Could not open socket\n");
    }

    //Get server info
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        raise_error("ERROR: Host does not exist\n");
    }

    //Assign file to encrypt
    filename = argv[3];

    memset ((char *) &server_address, '\0', sizeof(server_address));
    server_address.sin_family = AF_INET;
    memcpy ((char *)server->h_addr, (char *)&server_address.sin_addr.s_addr, server->h_length);
    server_address.sin_port = htons (portno);

    // Connect to server
    if (connect (socketfd, (struct sockaddr *) &server_address, sizeof (server_address)) < 0) 
        raise_error("ERROR: Couldn't connect to server");

    //File credentials
    FILE * file = fopen(filename, "r");
    char * buffer = NULL;
    off_t size;
    size_t read_res;

    //Get file information
    if(file!=NULL){
        struct stat st;
        if (stat(filename, &st) == 0){
            size = st.st_size;
            buffer = (char *)malloc(size+1);

            read_res = fread(buffer, sizeof(char), size, file); //read file into buffer

            if (read_res == 0) {
                free(buffer);
                fclose(file);
                raise_error("Error reading file\n");
            } else {
                buffer[size] = '\0'; 
            }

            /* Send file to server */
            int write_res = 0;
            while(write_res != read_res){
                 res = write (socketfd, buffer, read_res);
                 checkIO(res, &buffer, socketfd);
                 write_res+=res;
            }

        } else{
            printf("Stat failed!\n");
            fclose(file);
            exit(1);
        }    
    }else{
        printf("Opening file failed!\n");
        fclose(file);
        exit(1);
    }
    fclose(file);

    /* 

        Although not necessary, decided to hash file locally to see if the server sends correct
        data back.

    */
    CC_SHA256_CTX hash;
    CC_SHA256_Init(&hash);
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256_Update(&hash, buffer, read_res); 
    CC_SHA256_Final(digest, &hash);

    memset(buffer, '\0', size+1); //reset buffer

    /* Retrieve file */
    int reader_res = 0;
    while(reader_res != CC_SHA256_DIGEST_LENGTH){
        res = read (socketfd, buffer, CC_SHA256_DIGEST_LENGTH); //get hashed file from server

        checkIO(res, &buffer, socketfd);
        reader_res+=res;
    }

    //Buffer to hold response to server
    char response[BUFFERLENGTH];
    response[BUFFERLENGTH-1]='\0';

    //If hashes are equal
    if(memcmp(digest, buffer, CC_SHA256_DIGEST_LENGTH)==0){
        printf("Match\n");
        char * str = "File received and hash is correct";
        strcpy(response, str);
    }
    else{
        printf("No match\n");
        char * str = "File received and hash is not correct";
        strcpy(response, str);
    }
    response[BUFFERLENGTH-1]='\0';
    
    //Send response
    res = write (socketfd, response, strlen(response));
    checkIO(res, &buffer, socketfd);

    free(buffer);
    return 0;
}