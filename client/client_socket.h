#ifndef CLIENT_SOCKET_H_INCLUDED
#define CLIENT_SOCKET_H_INCLUDED

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <err.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../common/command_processor.h"

int hmdp_client_init(const char*, const char*);

char* send_message(const int, const char*);

#endif // CLIENT_SOCKET_H_INCLUDED
