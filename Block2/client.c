#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>


#define MAXDATASIZE 512 // max number of bytes we can get at once


int main(int argc, char *argv[])
{
    // declare Variables
    int sockfd, numbytes;
    //char *buffer = malloc(MAXDATASIZE* sizeof(char));
    char buf[MAXDATASIZE];
    struct addrinfo hints, *res, *p;
    int status;

    // check if enough arguments were given
    if (argc != 3) {
        fprintf(stderr,"pls enter client hostname and port\n");
        exit(1);
    }

    // get parameters for addrinfo; works with IPv4 and IPv6; Stream socket
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // get addrinfo
    if ((status = getaddrinfo(argv[1], argv[2], &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = res; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if ((status = connect(sockfd, p->ai_addr, p->ai_addrlen)) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }
        break;
    }

    // print error if connection failed
    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    freeaddrinfo(res); // all done with this structure

    // call recv function and print out answer, as long as server sends bytes
    while ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) > 0) {
        fwrite(buf, sizeof(char), numbytes, stdout);
        if(numbytes == -1){
            perror("recv");
            exit(1);
        }
    }

    //printf("\n");

    // close socket
    close(sockfd);

    return 0;
}
