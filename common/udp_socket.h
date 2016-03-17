#ifndef UDP_SOCKET_H
#define UDP_SOCKET_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <err.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <unistd.h>
#include <syslog.h>
#include <stdint.h>
#include <poll.h>

#define UDP_MSS 1472            //1500 - headers
#define MSG_BUFFER_SIZE         UDP_MSS - 6
#define MSG_CONTROL_BUFFER_SIZE UDP_MSS - 32
#define MSG_DATA_BUFFER_SIZE    UDP_MSS - 8

#define AUTH_TOKEN_LEN 16

#define TYPE_CONTROL_INIT 1
#define TYPE_CONTROL_TERM 2
#define TYPE_DATA 3
#define TYPE_ACK 255

//output every 10 segs
#define OUTPUT_INTERVAL 14720

typedef struct {
    uint32_t length;
    uint8_t buffer[MSG_BUFFER_SIZE];
} msg;

//request
typedef struct {
    uint32_t length;
    uint8_t type;               //1 for init, 2 for term
    uint8_t seq;
    uint16_t data_len;
    uint32_t file_size;
    uint32_t checksum;
    uint8_t auth_token[AUTH_TOKEN_LEN];
    uint8_t buffer[MSG_CONTROL_BUFFER_SIZE];
} msg_control;

//request
typedef struct {
    uint32_t length;
    uint8_t type;   //3
    uint8_t seq;
    uint16_t data_len;
    uint8_t buffer[MSG_DATA_BUFFER_SIZE];
} msg_data;

//response
typedef struct {
    uint32_t length;
    uint8_t type;       //255
    uint8_t seq;
    uint16_t err_code;  //1 if err
} msg_response;

typedef struct {
    struct sockaddr_in addr;
    socklen_t addr_len;
    char friendly_ip[INET_ADDRSTRLEN];
} host;

struct addrinfo* get_udp_sockaddr(const char* node, const char* port, int flags);

//used for hmds
struct addrinfo* get_tcp_sockaddr(const char* hostname, const char* port);
int open_tcp_connection(struct addrinfo* addr_list);

msg* receive_message(int sockfd, host* source);
int send_msg(int sockfd, msg* msg, host* source);

char* get_msg_type(int type);

#endif // UDP_SOCKET_H
