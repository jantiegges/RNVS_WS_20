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

#define BUFSIZE 16

typedef struct header{
    uint8_t command_line;
    uint16_t key_len;
    uint32_t val_len;
}header;

struct header *header_struct;


// get command from input console and set to appropiate integer
int get_command(char *argv[], uint8_t *command){
    if(strcmp(argv[3],"DELETE") == 0) *(command) = 1;
    else if(strcmp(argv[3],"SET") == 0) *(command) = 2;
    else if(strcmp(argv[3],"GET") == 0) *(command) = 4;
    else {
        printf("Invalid Input for command %s. Use GET, SET or DELETE\n", argv[3]);
        exit(1);
    }
    return 0;
}

char* marshalling(int argc, char *argv[], int *packet_size){

    // set key and key length
    char *key = malloc(strlen(argv[4]) * sizeof(char));
    uint16_t key_len;
    key = argv[4];
    key_len = strlen(argv[4]);

    // get command as uint8
    uint8_t command;
    get_command(argv, &command);

    // declare parameters for value
    int read_bytes;
    int counter = 0;
    uint32_t val_len = 0;
    char *buf = malloc(BUFSIZE * sizeof(char));
    char *val = malloc((BUFSIZE*2) * sizeof(char));

    // set value and length according to command
    // GET OR DELETE
    if(command == 1 || command == 4){
        buf = NULL;
        val_len = 0;
        //SET
    }else{
        while(1){
            read_bytes = fread(buf, sizeof(char), BUFSIZE, stdin);
            // if read bytes is smaller than buf_size we reach the EOF
            if(read_bytes < BUFSIZE){
                // set final value and value length
                val_len += read_bytes;
                val = realloc(val, val_len*sizeof(char));
                memcpy(val + counter, buf, read_bytes);
                break;
            }
            // set value length and allocate more storage for value
            val_len += read_bytes;
            val = realloc(val, val_len*sizeof(char));

            // set value and reset buffer
            memcpy(val + counter, buf, read_bytes);
            memset(buf, 0, BUFSIZE);

            counter += read_bytes;


        }
    }


    // declare and initialise packet
    *(packet_size) = 7 + key_len + val_len;
    char *packet;
    packet = malloc(*(packet_size) * sizeof(char));
    memset(packet, 0, *packet_size);

    // transform to network byte order
    uint16_t key_len_netorder = htons(key_len);
    uint32_t val_len_netorder = htonl(val_len);

    // make packet in correct byte order
    memcpy(packet, &command, 1);
    memcpy(packet+1, &key_len_netorder, 2);
    memcpy(packet + 3, &val_len_netorder,4);
    memcpy(packet + 7, key, key_len);
    memcpy(packet + 7 + (key_len), val, val_len);

    // is not needed anymore
    free(buf);
    free(val);

    return packet;
}

struct header *unmarshall(unsigned char h[]){
    struct header *header_struct = malloc(sizeof(header));
    header_struct->command_line = h[0];

    header_struct->key_len = h[1] << 8;
    header_struct->key_len += h[2];

    uint32_t tmp;
    header_struct->val_len = h[3] << 24;
    tmp = h[4] << 16;
    header_struct->val_len += tmp;
    tmp = h[5] << 8;
    header_struct->val_len += tmp;
    header_struct->val_len += h[6];
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

int main(int argc, char *argv[])
{

    // Code from “Beej’s Guide to Network Programming v3.1.5”, Chapter “A Simple Stream Client” was used

    // declare Variables for server connection
    int sockfd;
    struct addrinfo hints, *res, *p;
    int status;

    //declare variables for packet
    char *packet;
    int packet_size;

    packet = marshalling(argc, argv, &packet_size);

    // set parameters for addrinfo; works with IPv4 and IPv6; Stream socket
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

    // send packet to server
    if((status = send(sockfd, packet, packet_size, 0)) == -1){
        perror("client: send");
        exit(1);
    }

    // receive header first
    unsigned char header_recv[7];
    int numbytes = 0;
    if((numbytes = recv(sockfd, header_recv, 7, 0)) < 0) {
        perror("client: recv");
    }


    header_struct = malloc(sizeof(header));
    header_struct = unmarshall(header_recv);

    // check if request got acknowledged
    if(header_struct->command_line & (uint8_t)8){

        // only continue receiving if get
        if(header_struct->command_line & (uint8_t)4 ){

            // receive key and value
            unsigned char *key = calloc(header_struct->key_len + 1, 1);
            unsigned char *val = calloc(header_struct->val_len + 1, 1);

            recvall(sockfd, key, (uint32_t) header_struct->key_len);
            recvall(sockfd, val, header_struct->val_len);

            // write answer in stdout
            fwrite(val, sizeof(char), header_struct->val_len, stdout);

            // free reserved variables
            free(key);
            free(val);
        }


    }else{
        perror("request not acknowledged");
    }

    // close socket
    close(sockfd);

    // free reserved variables
    free(header_struct);


    return 0;
}
