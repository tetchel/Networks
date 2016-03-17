#ifndef SOCKET_H_INCLUDED
#define SOCKET_H_INCLUDED

#include "../hdb/hdb.h"
#include "../common/command_processor.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <err.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <signal.h>

void hmdp_server_init(const char*, hdb_connection* con);

void term_handler(int);

#endif // SOCKET_H_INCLUDED
