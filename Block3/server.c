#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include "hash_functions.h"

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
#define BACKLOG 10
#define DEL 0b0001
#define SET 0b0010
#define GET 0b0100
#define ACK 0b1000


typedef struct package{
    uint8_t ack;
    uint8_t cmd;
    uint16_t key_length;
    uint32_t value_length;
    char* key;
    void* value;
}package;

// print package information to stdout
void print_pkg(package* p) {
    fprintf(stdout, "Decoded packet header:\n");
    fprintf(stdout, "\tACK: %d\n", p->ack == ACK);
    fprintf(stdout, "\tGET: %d\n", p->cmd == GET);
    fprintf(stdout, "\tSET: %d\n", p->cmd == SET);
    fprintf(stdout, "\tDEL: %d\n", p->cmd == DEL);
    fprintf(stdout, "\tKey Length: %u Bytes\n", p->key_length);
    fprintf(stdout, "\tValue Length: %u Bytes\n", p->value_length);
}

// calculate byte-size of package
int pbytes(package* p) {
    return 7 + p->key_length + p->value_length;
}

// convert package data according to protocol
unsigned char* pack(package* p) {
    unsigned char* packed = calloc(pbytes(p), 1);
    packed[0] = p->ack + p->cmd;
    packed[1] = p->key_length >> 8;
    packed[2] = p->key_length;
    packed[3] = p->value_length >> 24;
    packed[4] = p->value_length >> 16;
    packed[5] = p->value_length >> 8;
    packed[6] = p->value_length;
    memcpy(packed + 7, p->key, p->key_length);
    memcpy(packed + 7 + p->key_length, p->value, p->value_length);
    return packed;
}

int recvall(int fd, unsigned char* buf, uint32_t len) {
    int total = 0;
    int bytesleft = len;
    int n;
    while (total < len) {
        n = recv(fd, buf + total, bytesleft, 0);
        if (n == -1) { break;}
        total += n;
        bytesleft -= n;

    }
    return n == -1 ? -1 : 0;
}

int sendall(int s, unsigned char *buf, int *len) {

    int total = 0;
// how many bytes we've sent

    int bytesleft = *len; // how many we have left to send
    int n;
    while (total < *len) {
        n = send(s, buf + total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }
    return n == -1 ? -1 : 0; // return -1 on failure, 0 on success
}


int get_header(unsigned char h[], package* p){
    int tmp;
    if ((h[0] & DEL) + (h[0] & SET)/SET + ((h[0] & GET)/GET) != 1)
        return -1;

    p->cmd = h[0] & 0b111;
    p->key_length = h[1] << 8;
    p->key_length += h[2];
    p->value_length = h[3] << 24;
    tmp = h[4] << 16;
    p->value_length += tmp;
    tmp = h[5] << 8;
    p->value_length += tmp;
    p->value_length += h[6];

    if (!p->key_length)
        return -1;

    return 0;
};

void sigchld_handler(int s)
{
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

int main (int argc, char *argv[]) {
    // Code from “Beej’s Guide to Network Programming v3.1.5”, Chapter “A Simple Stream Server” was used


    if (argc != 2) {
        fprintf(stderr,"usage: server port\n");
        exit(1);
    }

    hash_item* h = new_hash();

    char *port = argv[1];
    int sockfd, new_fd;
    int yes = 1;
    struct addrinfo hints, *servinfo, *p;
    struct sigaction sa;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    int rv;

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


        package* p = calloc(1, sizeof(package));
        unsigned char buf[7];

        if (recv(new_fd, buf, 7, 0) < 7)
            perror("receive");

        get_header(buf, p);
        p->key = calloc(p->key_length + 1, 1);
        p->value = calloc(p->value_length + 1, 1);
        recvall(new_fd, p->key, (uint32_t) p->key_length);
        recvall(new_fd, p->value, p->value_length);
        print_pkg(p);

        package* s = calloc(sizeof(package), 1);
        s->cmd = p->cmd;
        if (p->cmd == GET) {
            s->key_length = p->key_length;
            s->key = malloc(s->key_length);
            memcpy(s->key, p->key, s->key_length);
        }
        switch(p->cmd) {
            case GET: s->ack = get_item(&h, p->key, p->key_length, &(s->value), &(s->value_length)); break;
            case SET: s->ack = set_item(&h, p->key, p->value, p->key_length, p->value_length); break;
            case DEL: s->ack = delete_item(&h, p->key, p->key_length); break;
        }

        unsigned char* packed_data = pack(s);
        int len = pbytes(s);
        sendall(new_fd, packed_data, &len);
        free(packed_data);
        close(new_fd);

    }




}
#pragma clang diagnostic pop