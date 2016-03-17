#ifndef CLIENT_H_
#define CLIENT_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <zlib.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <pwd.h>
#include <syslog.h>
#include "../common/command_processor.h"

void traverseDir(DIR*, char*, int, FILE*);
char* appendStr(char*, char*, bool);
uLong computeChecksum(char*);

#endif /* CLIENT_H_ */
