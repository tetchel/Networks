#ifndef HFTPD_CLIENT_OPS_H
#define HFTPD_CLIENT_OPS_H

#include "../common/udp_socket.h"
#include "hftpd_client_socket.h"

#include <hfs.h>

void client_operations(int sockfd, host* host, hfs_entry* files, char* auth_token, char* username, int num_files);
msg* data_from_file(char* abs_path, uint32_t f_size, uint8_t seq_num, int* index);

#endif // HFTPD_CLIENT_OPS_H
