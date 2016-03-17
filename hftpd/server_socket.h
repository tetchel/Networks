#ifndef SERVER_SOCKET_H
#define SERVER_SOCKET_H

#include "../common/udp_socket.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <hdb.h>

void hftp_server(const char* redis_server, const char* port, char* dir, const char* timewait);
int bind_socket(struct addrinfo* results);

bool get_term_status();
void term_handler(int sig);

int ack(uint8_t seq_num, int sockfd, host* source, bool error);

bool time_wait(uint8_t seq_num, int sockfd, host* dest, const char* timewait);

int append_to_file(char* dir, char* rel_path, uint8_t* buffer, uint16_t buff_len);

#endif // SERVER_SOCKET_H
