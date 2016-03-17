#include "hftpd_client_socket.h"

//arbitrary number of times to resend while waiting for ACK
#define MAX_TRIES 120

//server is out param. taken from lab
int bind_socket(const char* hostname, const char* port, host* server) {
    int sockfd;
    struct addrinfo* addr;
    struct addrinfo* info_result = get_udp_sockaddr(hostname, port, 0);

    for(addr = info_result; addr != NULL; addr = addr->ai_next) {
        sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

        if(sockfd == -1)
            continue;

        //success
        //copy server addr, length into server param
        memcpy(&server->addr, addr->ai_addr, addr->ai_addrlen);
        memcpy(&server->addr_len, &addr->ai_addrlen, sizeof(addr->ai_addrlen));
        break;
    }

    freeaddrinfo(info_result);

    if(!addr) {
        errx(EXIT_FAILURE, "Unable to create socket");
    }
    else
        return sockfd;
}

//loops until ACK with correct seq # arrives, or until timeout
int wait_for_ack(msg* msg, int sockfd, int seq_num, host* host) {
    //wait for ack, if timeout resend
    struct pollfd fd = {
        .fd = sockfd,
        .events = POLLIN
    };
    int tries = 0;
    //wait until ACK
    while(tries++ < MAX_TRIES) {
        int poll_status = poll(&fd, 1, 1000);
        if(poll_status == 1 && fd.revents == POLLIN) {
            msg_response* response = (msg_response*) receive_message(sockfd, host);
            syslog(LOG_DEBUG, "Received ACK: Sequence # %d, Error field %d", response->seq, response->err_code);
            //good ACK! return the error code
            if(response->seq == seq_num) {
                uint16_t errcode = response->err_code;
                free(response);
                return errcode;
            }
            free(response);
        }
        syslog(LOG_DEBUG, "Waiting for ACK %d", seq_num);
        //wrong sequence number, else it would have returned. resend
        send_msg(sockfd, msg, host);
    }
    syslog(LOG_EMERG, "Timed out after resending %d times without receiving ACK.", MAX_TRIES);
    //not executed unless timeout (shouldn't happen)
    return 1;
}

//builds a type 1 control request from the given info
msg* build_control_init_request(uint8_t seq, uint16_t len, uint32_t file_size, uint32_t checksum, char auth_token[AUTH_TOKEN_LEN], char* buffer) {
    msg_control* result = (msg_control*)malloc(sizeof(msg_control));
    result->type = TYPE_CONTROL_INIT;
    result->seq  = seq;
    //it is essential that len is correct
    result->data_len = htons(len);
    result->file_size  = htonl(file_size);
    result->checksum   = htonl(checksum);
    memcpy(result->auth_token, auth_token, AUTH_TOKEN_LEN);
    memcpy(result->buffer, buffer, len);

    result->length = sizeof(result->type) + sizeof(seq) + sizeof(len) + sizeof(file_size) + sizeof(checksum) + AUTH_TOKEN_LEN + strlen(buffer);
    //4 + 1 + 1 + 2 + 4 + 4 + 16 + buffer
//    result->length = htonl(result->length);
    return (msg*)result;
}

//builds a data request from the given info
msg* build_data_request(uint8_t seq, uint16_t len, char* buffer) {
    msg_data* result = (msg_data*)malloc(sizeof(msg_data));
    result->type = TYPE_DATA;
    result->seq = seq;
    result->data_len = htons(len);
    memcpy(result->buffer, buffer, len);

    result->length = sizeof(result->type) + sizeof(seq) + sizeof(len) + strlen(buffer);
//    result->length = htonl(result->length);
    return (msg*)result;
}
