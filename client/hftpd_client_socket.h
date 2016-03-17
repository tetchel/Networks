#ifndef CLIENT_SOCKET_H
#define CLIENT_SOCKET_H

#include "../common/udp_socket.h"

int bind_socket(const char* hostname, const char* port, host* server);

int wait_for_ack(msg* msg, int sockfd, int seq_num, host* host);

msg* build_control_init_request(uint8_t seq, uint16_t len, uint32_t file_size,
                                uint32_t checksum, char auth_token[AUTH_TOKEN_LEN], char* buffer);

msg* build_data_request(uint8_t seq, uint16_t len, char* buffer);


#endif // CLIENT_SOCKET_H
