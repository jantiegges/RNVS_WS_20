#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <time.h>

#define BACKLOG 1


void sigchld_handler(int s)
{
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int main (int argc, char *argv[]) {
    //read qotd.txt

    char *line_buf = NULL;
    size_t line_buf_size = 0;
    int line_count = 0;
    ssize_t line_size;
    FILE *fp;

    // check if enough arguments were given
    if (argc != 3) {
        fprintf(stderr,"missing port and filename\n");
        exit(1);
    }

    fp = fopen(argv[2], "r");
    if (fp==NULL)
    {
        fprintf(stderr, "Error opening file '%s'\n", argv[2]);
        return EXIT_FAILURE;
    }

    // get number of lines in file
    line_size = getline(&line_buf, &line_buf_size, fp);
    while (line_size >= 0)
    {
        line_count++;
        line_size = getline(&line_buf, &line_buf_size, fp);
    }


    //
    int sockfd, new_fd;
    int yes = 1;
    struct addrinfo hints, *servinfo, *p;
    struct sigaction sa;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;


    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
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

    printf("server: waiting for connections...\n");

    while (1) {
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *) &their_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);

        fclose(fp);
        fp = fopen(argv[2], "r");

        if (!fork()) {
            close(sockfd);
            // takes random line
            time_t t;
            t = (unsigned) time(&t);
            srand(t);
            int r = rand() % line_count;

            // sends random line
            //TODO how to send random line
            //for(int i=0;i<r;i++) {
            line_buf_size = 0;
            line_buf = NULL;
            ssize_t tem = getline(&line_buf, &line_buf_size, fp);
            if (tem == -1) {
                perror("Server - Getline failed: ");
                exit(1);
            }
            //TODO How to handle strings with null-bytes bc strlen leaves them out
            int i;
            char quote[line_buf_size];
            int quote_length = 0;
            for (i = 0; i < strlen(line_buf); i++){
                if(line_buf[i] != '\n'){
                    quote[quote_length] = line_buf[i];
                    quote_length++;
                }
            }
            if (send(new_fd, quote, quote_length, 0) == -1)
                perror("send");
            close(new_fd);
            exit(0);
            //}
        }
        close(new_fd);

    }
    return 0;
}