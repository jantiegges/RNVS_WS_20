#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <time.h>

#define BACKLOG 10


void sigchld_handler(int s)
{
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}


int readfile(char* path, char*** quotes, int** quotelength, int* numquotes) {

    // temp variables for readout
    char *line_buffer = NULL;
    size_t line_buffer_size = 0;
    ssize_t line_size;
    FILE *fp;

    // open file
    if ((fp = fopen(path, "r")) == NULL) {
        perror("fopen: could not read file");
        return 1;
    }

    // determine number of lines
    *numquotes = 0;
    while(1) {
        line_size = getline(&line_buffer, &line_buffer_size, fp);
        if (line_size < 0)
            break;
        if (line_buffer[line_size - 1] == '\n')
            (*numquotes)++;
    }

    // reinitialize readout
    fclose(fp);
    free(line_buffer);
    line_buffer = NULL;
    line_buffer_size = 0;
    fp = fopen(path, "r");

    // allocate memory
    (*quotelength) = (int *) calloc(*numquotes, sizeof(int));
    (*quotes) = (char **) calloc(*numquotes, sizeof(char*));

    // copy file content
    for (int i = 0; i < *numquotes; i++) {
        line_size = getline(&line_buffer, &line_buffer_size, fp);
        (*quotelength)[i] = line_size - 1;
        (*quotes)[i] = (char *) calloc(line_size, sizeof(char));
        memcpy((*quotes)[i], line_buffer, sizeof(char) * (line_size - 1));
    }

    // cleanup
    free(line_buffer);
    fclose(fp);


}


int main (int argc, char *argv[]) {
	// Code from “Beej’s Guide to Network Programming v3.1.5”, Chapter “A Simple Stream Server” was used


    if (argc != 3) {
        fprintf(stderr,"usage: server port path/quotes.txt\n");
        exit(1);
    }

    char *port = argv[1];
    char *path = argv[2];
    char **quotes;
    int *quotelength;
    int numquotes;
    int sockfd, new_fd;
    int yes = 1;
    struct addrinfo hints, *servinfo, *p;
    struct sigaction sa;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    int rv;

    readfile(path, &quotes, &quotelength, &numquotes);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;



    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
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

    freeaddrinfo(servinfo);

    if (p == NULL) {
        fprintf(stderr, "server: failed do bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }


    while(1) {
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        if (!fork()) {
            close(sockfd);
            time_t t;
            t = (unsigned) time(&t);
            srand(t);
            int r = rand() % numquotes;
            if (send(new_fd, quotes[r], quotelength[r], 0) == -1)
                perror("send");
            close(new_fd);
            free(quotelength);
            for (int i = 0; i < numquotes; i++)
                free(quotes[i]);

            free(quotes);

            exit(0);
        }
        close(new_fd);
    }




}