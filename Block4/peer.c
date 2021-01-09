#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include "hash_functions.h"

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
#define BACKLOG 10
#define DEL 0b0001
#define SET 0b0010
#define GET 0b0100
#define CTL 0b10000000
#define ACK 0b1000
#define RPL 0b10
#define LKP 0b01

typedef struct package{
    int from_sock;
    uint8_t ack;
    uint8_t cmd;
    uint16_t key_length;
    uint32_t value_length;
    char* key;
    void* value;
}package;


typedef struct node{
    uint16_t id;
    unsigned long ip;
    int port;
}node;

typedef struct ctl_package{
    uint8_t cmd;
    uint8_t lkp;
    uint16_t hash_id;
    node nd;

}ctl_package;

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

    int bytesleft = *len;
    int n;
    while (total < *len) {
        n = send(s, buf + total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }
    return n == -1 ? -1 : 0; //
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

int ctl_get_package(unsigned char d[], ctl_package* p) {
    if ((d[0] & LKP) + (d[0] & RPL)/RPL != 1)
        return -1;
    p->cmd = d[0] & 0b11;

    p->hash_id = d[1] << 8;
    p->hash_id += d[2];

    p->nd.id = d[3] << 8;
    p->nd.id += d[4];

    p->nd.ip += d[5] << 24;
    p->nd.ip += d[6] << 16;
    p->nd.ip += d[7] << 8;
    p->nd.ip += d[8];

    p->nd.port += d[9] << 8;
    p->nd.port += d[10];

}

void sigchld_handler(int s)
{
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

int set_node(node* node, char* id, char* ip, char* port) {
    struct in_addr tmp;
    inet_pton(AF_INET, ip, &tmp);
    node->id = atoi(id);
    node->ip = tmp.s_addr;
    node->port = atoi(port);
    return 0;
}

int my_bin(ctl_package p, node self, node pred) {
    if (self.id > pred.id) {
        return (int) (p.hash_id > pred.id && p.hash_id <= self.id);
    } else {
        return (int) (p.hash_id > pred.id || p.hash_id <= self.id);
    }
}

int reply(ctl_package p, node self) {

}

int lookup(ctl_package ctl_p, node succ) {
    int sockfd = socket(AF_UNSPEC, SOCK_STREAM, 0);
    connect(sockfd, &(succ.ip), INET_ADDRSTRLEN); // nicht sicher, ob das so geht...
    unsigned char packet[11];
    packet[0] = CTL;
    packet[0] += LKP;
    packet[1] = ctl_p.hash_id >> 8;
    packet[2] = ctl_p.hash_id;
    packet[3] = ctl_p.nd.id >> 8;
    packet[4] = ctl_p.nd.id;
    packet[5] = ctl_p.nd.ip >> 24;
    packet[6] = ctl_p.nd.ip >> 16;
    packet[7] = ctl_p.nd.ip >> 8;
    packet[8] = ctl_p.nd.ip;
    packet[9] = ctl_p.nd.port >> 8;
    packet[10] = ctl_p.nd.port;
    send(sockfd, packet, 11, 0);
    close(sockfd);
}

int main (int argc, char *argv[]) {
    // Code from “Beej’s Guide to Network Programming v3.1.5”, Chapter “A Simple Stream Server” was used


    if (argc != 10) {
        fprintf(stderr,"usage: peer node_id node_ip node_port pred_id pred_ip pred_port succ_id succ_ip succ_port\n");
        exit(1);
    }

    //parse arguments

    node node_self, node_pred, node_succ;
    set_node(&node_self, argv[1], argv[2], argv[3]);
    set_node(&node_pred, argv[4], argv[5], argv[6]);
    set_node(&node_succ, argv[7], argv[8], argv[9]);


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


        unsigned char h;

        if (recv(new_fd, &h, 1, 0) < 1)
            perror("receive");

        char ctl_msg = (h & CTL) / CTL;
        if (ctl_msg) {
            unsigned char buf[11];
            buf[0] = h;
            ctl_package p;

            if (recv(new_fd, buf + 1, 10, 0) < 10)
                perror("receive");
            ctl_get_package(buf, &p);
            close(new_fd);

            if (p.cmd == LKP) {
                if (my_bin(p, node_self, node_pred)) {
                    reply(p, node_self);

                } else {
                    lookup(p, node_succ);

                }
            }




        } else {

            unsigned char buf[7];
            buf[0] = h;
            package* p = calloc(1, sizeof(package));
            if (recv(new_fd, buf + 1, 6, 0) < 6)
                perror("receive");


            get_header(buf, p);
            p->from_sock = new_fd;
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




}
#pragma clang diagnostic pop