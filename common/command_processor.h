#ifndef COMMAND_PROCESSOR_H_INCLUDED
#define COMMAND_PROCESSOR_H_INCLUDED

#include "../hdb/hdb.h"
#include <err.h>

//need to make this bigger for running on large directories
#define BUFFER_SIZE 4096

typedef struct key_value_t {
    char* key;
    char* value;
    struct key_value_t* next;
} key_value_t;

typedef enum {ERR, AUTH, LIST} request_types_e ;
typedef enum {ERR2, AUTH_SUCC, NO_FILES, FILES_REQ, AUTH_FAIL} response_types_e;

char* read_until(char*, char);
char* build_command(char*, key_value_t*, char*);

request_types_e get_request_type(char*, int*);
response_types_e get_response_type(char*, int*);

int extract_kv(char*, int, char**);

long extract_body(char*, int, char**);

#endif // COMMAND_PROCESSOR_H_INCLUDED
