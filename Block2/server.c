#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define BACKLOG 1   // how many pending connections queue will hold
#define MAXDATASIZE 512 // max number of bytes we send at once

#define MAX_NUM_QUOTES 50
#define MAX_QUOTELENGTH 512

int main(int argc, char *argv[])
{
    // declare variables
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *res, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    int yes=1;
    int status;

    FILE *fptr;
    int line_counter = 0;


    // check if enough arguments were given
    if (argc != 3) {
        fprintf(stderr,"missing port and filename\n");
        exit(1);
    }

    // TODO
    // check if port number is in range
    //int port_int = atoi(argv[1]);

    // open file in read mode and check for error
    //fptr = fopen(argv[2], "r");
    fptr = fopen("../qotd.txt", "r");

    if(fptr == NULL){
        perror("Server - File did not open: ");
        exit(1);
    }


    // set parameters for addrinfo; works with IPv4 and IPv6; Stream socket
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    // get addrinfo
    if ((status = getaddrinfo(NULL, argv[1], &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = res; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if ((status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int))) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }

    freeaddrinfo(res); // all done with this structure

    // print error if binding failed
    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    // listen
    if ((status =listen(sockfd, BACKLOG)) == -1) {
        perror("listen");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    //Get number of lines in document
    char c;
    for (c = getc(fptr); c != EOF; c = getc(fptr)){
        if (c == '\n') // Increment count if this character is newline
            line_counter++;
    }

    char quotes[MAX_NUM_QUOTES][MAX_QUOTELENGTH];
    int quote_length[MAX_QUOTELENGTH];

    int i = 0;
    int j = 0;

    // saves quotes and length of quotes in array
    while(fgets(quotes[i], MAX_QUOTELENGTH, fptr)){
        while(j<MAX_QUOTELENGTH && quotes[i][j] != "\n"){
            j++;
        }
        quote_length[i] = j;
        i++;
    }

    line_counter = i;


    // always stay ready for connection
    while(1) {
        // creates a new socket for connecting client
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        // create random number of a line in document
        int rand_line = rand() % line_counter;

        if ((status = send(new_fd, quotes[rand_line], quote_length[rand_line], 0)) == -1)
            perror("send");

        close(new_fd);
        exit(0);

    }
    return 0;
}