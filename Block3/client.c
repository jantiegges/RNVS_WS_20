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
    int val_len = 0;
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
                val_len = (uint32_t)val_len;
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

            //debug comment: richtige value length aber verdrehte ausgabe
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

    // debug code
    // char test[15];
    // memcpy(test, packet, 15);

    return packet;
}

struct header *unmarshall(char *header_recv){
    struct header *header_struct = malloc(sizeof(header));
    header_struct->command_line = header_recv[0];
    header_struct->key_len = (header_recv[1]<<8 | header_recv[2]);
    header_struct->val_len = (header_recv[3]<<24 | header_recv[4]<<16 | header_recv[5]<<8 | header_recv[6]);
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

    // receive answer from server
    char *header_recv = malloc(7 * sizeof(char));
    char *buf = malloc(sizeof(char));
    int numbytes = 0;
    int count_recv = 0;
    // receive header first
    while((numbytes = recv(sockfd, buf, 1, 0)) != 0){
        if(numbytes == -1){
            perror("client: recv");
        }

        memcpy(header_recv + count_recv, buf, numbytes);
        count_recv += numbytes;
        // break if header is received
        if(count_recv == 7) break;
    }


    header_struct = malloc(sizeof(header));
    header_struct = unmarshall(header_recv);

    // check if request got acknowledged
    if(header_struct->command_line & (uint8_t)8){

        // only continue receiving if get
        if(header_struct->command_line & (uint8_t)4 ){
            // receive body
            int body_length = header_struct->val_len + header_struct->key_len;
            char *body_buf = malloc(body_length * sizeof(char));
            char *body = malloc(body_length * sizeof(char));
            numbytes = 0;
            count_recv = 0;

            while((numbytes = recv(sockfd, body_buf, body_length, 0)) != 0){
                if(numbytes == -1){
                    perror("client: recv");
                }
                memcpy(body + count_recv, body_buf, numbytes);
                count_recv += numbytes;
                if(count_recv == body_length) break;
            }

            // safe key and value
            char *key = malloc(header_struct->key_len * sizeof(char));
            char *val = malloc(header_struct->val_len * sizeof(char));
            memcpy(key, body, header_struct->key_len);
            memcpy(val, body + header_struct->key_len, header_struct->val_len);

            // write answer in terminal
            fwrite(val, sizeof(char), header_struct->val_len, stdout);

            printf("%d", header_struct->val_len);

            // free reserved variables
            free(body_buf);
            free(body);
            free(key);
            free(val);
        }


    }else{
        perror("request not acknowledged");
    }

    // close socket
    close(sockfd);

    // free reserved variables
    free(buf);
    free(header_recv);
    free(header_struct);


    return 0;
}
