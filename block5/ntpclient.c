/*
** listener.c -- a datagram sockets "server" demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <signal.h>
#include <stdbool.h>
#include <math.h>

#define MYPORT "123"    // the port users will be connecting to

#define TIMESTAMP_DELTA 2208988800ull

typedef struct ntp_packet{
    uint8_t li;
    uint8_t vn;
    uint8_t mode;
    uint8_t stratum;
    uint8_t poll;
    uint8_t precision;
    uint32_t root_delay;
    uint16_t root_dispersion_ts;
    uint16_t root_dispersion_tf;
    uint32_t reference_id;
    uint32_t reference_ts; // seconds
    uint32_t reference_tf; // fraction
    uint32_t origin_ts;
    uint32_t origin_tf;
    uint32_t recv_ts;
    uint32_t recv_tf;
    uint32_t transmit_ts;
    uint32_t transmit_tf;
}ntp_packet;

// method for serializing the raw packet from packet struct
unsigned char* packet_serialize(const ntp_packet* p){
    size_t packet_size = 48;
    unsigned char* buffer = (unsigned char*) malloc(packet_size);
    memset(buffer, 0, sizeof(buffer));

    buffer[0] = buffer[0] | (p->vn << 3u);
    buffer[0] = buffer[0] | p->mode;

    return buffer;
}

// method for decoding the packet struct from raw packet
struct ntp_packet* packet_decode(unsigned char* raw_recv){
    struct ntp_packet* buffer = malloc(sizeof(ntp_packet));
    buffer->li = raw_recv[0]>>6;
    // and with 7 in order to erase first 5 bits of byte
    buffer->vn = (raw_recv[0]>>3)&7;
    buffer->mode = raw_recv[0]&7;
    buffer->stratum = raw_recv[1];
    buffer->poll = raw_recv[2];
    buffer->precision = raw_recv[3];

    memcpy(&buffer->root_delay, raw_recv+4, sizeof(u_int32_t));
    memcpy(&buffer->root_dispersion_ts, raw_recv+8, sizeof(u_int16_t));
    memcpy(&buffer->root_dispersion_tf, raw_recv+10, sizeof(u_int16_t));
    memcpy(&buffer->recv_ts, raw_recv+32, sizeof(u_int32_t));
    memcpy(&buffer->recv_tf, raw_recv+36, sizeof(u_int32_t));
    memcpy(&buffer->transmit_ts, raw_recv+40, sizeof(u_int32_t));
    memcpy(&buffer->transmit_tf, raw_recv+44, sizeof(u_int32_t));

    buffer->root_delay = ntohl(buffer->root_delay);
    buffer->root_dispersion_ts = ntohs(buffer->root_dispersion_ts);
    buffer->root_dispersion_tf = ntohs(buffer->root_dispersion_tf);
    buffer->recv_ts = ntohl(buffer->recv_ts);
    buffer->recv_tf = ntohl(buffer->recv_tf);
    buffer->transmit_ts = ntohl(buffer->transmit_ts);
    buffer->transmit_tf = ntohl(buffer->transmit_tf);

    return buffer;
}

uint64_t timespecToNanoseconds(struct timespec* ts){
    return ((uint64_t)ts->tv_sec * (uint64_t)1000000000) + (uint64_t)ts->tv_nsec;
}

double calculateDelay(struct timespec t1, struct timespec t2, struct timespec t3, struct timespec t4){
    uint64_t t1_nano = timespecToNanoseconds(&t1);
    uint64_t t2_nano = timespecToNanoseconds(&t2);
    uint64_t t3_nano = timespecToNanoseconds(&t3);
    uint64_t t4_nano = timespecToNanoseconds(&t4);

    uint64_t buf = (uint64_t) ((t4_nano-t1_nano)-(t3_nano-t2_nano)) / (uint64_t)2;

    return (double)buf * 0.000000001;
}

double calculateOffset(struct timespec t1, struct timespec t2, struct timespec t3, struct timespec t4){
    uint64_t t1_nano = timespecToNanoseconds(&t1);
    uint64_t t2_nano = timespecToNanoseconds(&t2);
    uint64_t t3_nano = timespecToNanoseconds(&t3);
    uint64_t t4_nano = timespecToNanoseconds(&t4);

    uint64_t buf = (uint64_t) ((t2_nano-t1_nano)+(t3_nano-t4_nano)) / (uint64_t)2;

    return (double)buf * 0.000000001;
}

double calculateDispersion(int j, double delay[]){
    double max = delay[0];
    double min = delay[0];

    if(j<7){
        for(int i=1; i<=j; i++){
            if(delay[i]>max){
                max = delay[i];
            }
            if(delay[i]<min){
                min = delay[i];
            }
        }
    }else{
        for(int i=j-7; i<=j; i++){
            if(delay[i]>max){
                max = delay[i];
            }
            if(delay[i]<min){
                min = delay[i];
            }
        }
    }
    double buf = max-min;

    return buf;
}


int main(int argc, char** argv)
{
    // declare Variables for server connection
    int sockfd;
    struct addrinfo hints, *res, *p;
    int status;
    socklen_t addr_len;
    struct timespec start_time, finish_time;
    size_t packet_size = 48;
    struct sockaddr_storage their_addr;


    // make the program ignore sigpipe
    // sigaction(SIGPIPE, &(struct sigaction){SIG_IGN}, NULL);


    // check if enough arguments were given
    if (argc < 3){
        fprintf(stderr, "Not enough args!\n");
        return -1;
    }

    // set number of requests and servers
    int num_requests = atoi(argv[1]);
    int num_servers = argc-2;
    char* server_arr[num_servers];

    for(int i=0; i<num_servers; i++){
        server_arr[i] = argv[i+2];
    }

    // declare variables for print function
    double delay[num_requests];
    double offset;
    double dispersion;
    double rootDispersion;

    // init packet
    struct ntp_packet *ntpp = malloc(sizeof(struct ntp_packet));

    // set NTP Version and mode
    ntpp->vn = (uint8_t)4;
    ntpp->mode = (uint8_t)3;

    unsigned char* raw_pkt = packet_serialize(ntpp);




    // iterate through servers
    for(int i = 0; i<num_servers; i++){
        // set parameters for addrinfo; works with IPv4 and IPv6; Stream socket
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC; // set to AF_INET to use IPv4
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE; // use my IP

        // get addrinfo
        if ((status = getaddrinfo(server_arr[i], MYPORT, &hints, &res)) != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
            return 1;
        }

        // loop through all the results and create socket for first one we can
        for(p = res; p != NULL; p = p->ai_next) {
            if ((sockfd = socket(p->ai_family, p->ai_socktype,
                                 p->ai_protocol)) == -1) {
                perror("client: socket");
                continue;
            }
            break;
        }

        // print error if creating the socket failed
        if (p == NULL) {
            fprintf(stderr, "client: failed to create socket\n");
            return 2;
        }

        // iterate through number of requests
        for(int j=0; j<num_requests; j++){

            unsigned char* raw_recv = (unsigned char*) malloc(packet_size);
            memset(raw_recv, 0, 48 * sizeof(char));
            addr_len = sizeof their_addr;

            // set origin time
            clock_gettime(CLOCK_REALTIME, &start_time);

            if ((status = sendto(sockfd, raw_pkt, packet_size, 0,
                                 p->ai_addr, p->ai_addrlen)) == -1) {
                perror("talker: sendto");
                exit(1);
            }

            if ((status = recvfrom(sockfd, raw_recv, packet_size, 0,
                                   (struct sockaddr *)&their_addr, &addr_len)) == -1) {
                perror("recvfrom");
                exit(1);
            }

            // set destination time
            clock_gettime(CLOCK_REALTIME, &finish_time);

            struct ntp_packet *rsp = packet_decode(raw_recv);

            // set timestamps
            struct timespec t1, t2, t3, t4;
            t1.tv_sec = start_time.tv_sec ;
            t1.tv_nsec = start_time.tv_nsec;


            // conversion from unix to ntp
            uint64_t convNtp;
            uint64_t buf;
            t2.tv_sec = rsp->recv_ts - TIMESTAMP_DELTA;
            convNtp = (uint64_t) rsp->recv_tf;
            buf = (convNtp*(uint64_t)1000000000) / (uint64_t)pow(2,32);
            t2.tv_nsec = buf;

            t3.tv_sec = rsp->transmit_ts - TIMESTAMP_DELTA;
            convNtp = (uint64_t) rsp->transmit_tf;
            buf = (convNtp*(uint64_t)1000000000) / (uint64_t)pow(2,32);
            t3.tv_nsec = buf;

            t4.tv_sec = finish_time.tv_sec;
            t4.tv_nsec = finish_time.tv_nsec;

            // calculate delay, offset and dispersion
            delay[j] = calculateDelay(t1,t2,t3,t4);
            offset = calculateOffset(t1,t2,t3,t4);
            dispersion = calculateDispersion(j, delay);

            // set rootDispersion
            struct timespec rd;
            rd.tv_sec = rsp->root_dispersion_ts;
            convNtp = rsp->root_dispersion_tf;
            buf = (convNtp*(uint64_t)1000000000) / (uint64_t)pow(2,16);
            rd.tv_nsec = buf;
            buf = rd.tv_sec * (uint32_t)1000000000L + rd.tv_nsec;
            rootDispersion = (double) buf / 1000000000;

            printf("%s;%i;%f;%f;%f;%f\n", server_arr[i], j, rootDispersion, dispersion, delay[j], offset);

            sleep(8);
        }

    }

    return 0;
}