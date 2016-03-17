#include "hdb.h"
#include "../common/command_processor.h"

//opens a connection to the hdb server and returns a pointer to it
hdb_connection* hdb_connect(const char* server) {
  //2s timeout (arbitrary)
	struct timeval timeout = {2, 0};
  //use port 6379
	redisContext* context = redisConnectWithTimeout(server, 6379, timeout);
  //see if an error occurred
	if(context == NULL || context->err) {
		if(context) {
			printf("Connection error: %s\n", context->errstr);
			redisFree(context);
		}
		else {
			printf("Connection error; unable to allocate redis context");
		}
		exit(1);
	}
  //everything went ok, return the connection
	else {
		return *(hdb_connection**)&context;
	}
}

//frees memory holding database connection
void hdb_disconnect(hdb_connection* con) {
    //free the context after undoing the cast
	redisFree(toRedisContext(con));
}

//store the given record in the server
void hdb_store_file(hdb_connection* con, hdb_record* record) {
    char* command;
    asprintf(&command, "HSET %s %s %s",record->username, record->filename, record->checksum);
    //we don't care about any return value so call runCommand directly, and free the reply
    freeReplyObject(runCommand(con, command));
}

//remove the given record from the server and return if it was successful
int hdb_remove_file(hdb_connection* con, const char* username, const char* filename) {
    char* command;
    asprintf(&command, "HDEL %s %s", username, filename);
    return runCommandInt(con, command);
}

char* hdb_file_checksum(hdb_connection* con, const char* username, const char* filename) {
  // If the specified file exists in the Redis server, return its checksum.
  // Otherwise, return NULL.
    char* command;
    asprintf(&command, "HGET %s %s", username, filename);
    return runCommandStr(con, command);
}

//returns the number of files the given user has stored on the server.
int hdb_file_count(hdb_connection* con, const char* username) {
    char* command;
    asprintf(&command, "HLEN %s", username);
    return runCommandInt(con, command);
}

//returns whether or not the user has one or more files on the server
bool hdb_user_exists(hdb_connection* con, const char* username) {
    char* command;
    asprintf(&command, "EXISTS %s", username);
    return runCommandInt(con, command);
}

//returns whether or not the given user has a file named filename on the server
bool hdb_file_exists(hdb_connection* con, const char* username, const char* filename) {
    char* command;
    asprintf(&command, "HEXISTS %s %s", username, filename);
    return runCommandInt(con, command);
}

//returns a linked list of all of the given user's files on the server
//NULL if the user has no files on the server
hdb_record* hdb_user_files(hdb_connection* con, const char* username) {
  //if they have no files return null
    if(hdb_file_count(con, username) == 0)
        return NULL;

    char* command;
    asprintf(&command, "HGETALL %s", username);

    redisReply* reply = runCommand(con, command);
    int length = reply->elements;
    //record being processed
    hdb_record* current;
    //returned value; first item in the list
    hdb_record* ret  = malloc(sizeof(struct hdb_record));
    //ret is the first item, start there
    current = ret;

    int i;
    for (i = 0; i < length; i++) {
        asprintf(&(current->username), "%s", username);
        asprintf(&(current->filename), "%s", reply->element[i]->str);
        //increment i to get the checksum
        asprintf(&(current->checksum), "%s", reply->element[++i]->str);

        if(i != length-1) {
            current->next = malloc(sizeof(struct hdb_record));
            current = current->next;
        }
        else {
            //last record points to NULL
            current->next = NULL;
        }
    }

    freeReplyObject(reply);
    return ret;
}

//frees the memory allocated in hdb_user_files()
void hdb_free_result(hdb_record* record) {
    hdb_record* next = record;
    while(next != NULL) {
        next = record->next;
        free(record->username);
        free(record->filename);
        free(record->checksum);
        free(record);
        record = next;
    }
}

//delete all data associated with a user on the server
int hdb_delete_user(hdb_connection* con, const char* username) {
    char* command;
    asprintf(&command, "DEL %s", username);
    return runCommandInt(con, command);
}

//////////////////// HELPER FUNCTIONS ////////////////////

//casts an hdb_connection* to a redisContext*
redisContext* toRedisContext(hdb_connection* con) {
    return *(redisContext**)&con;
}

//runs the command stored in command and returns its reply
//remember to free the reply if this is called directly
redisReply* runCommand(hdb_connection* con, char* command) {
    redisContext* c = toRedisContext(con);
    redisReply* reply = redisCommand(c, command);
    free(command);
    // printf("%s int: %lld str: %s\n", command, reply->integer, reply->str);
    return reply;
}

//runs a command and returns its reply's int value
int runCommandInt(hdb_connection* con, char* command) {
    redisReply* reply = runCommand(con, command);
    int i = reply->integer;
    freeReplyObject(reply);
    return i;
}

//runs a command and returns its reply's string value
char* runCommandStr(hdb_connection* con, char* command) {
    redisReply* reply = runCommand(con, command);
    if(reply->str != NULL) {
        char* s;
        asprintf(&s, "%s", reply->str);
        freeReplyObject(reply);
        return s;
    }
    else {
        freeReplyObject(reply);
        return NULL;
    }
}

//Assignment 2, part 0//
#define TOKEN_LEN 16
//returns a random 16-byte token, and associates it with the username in redis
char* get_db_token(hdb_connection* con, const char* username) {
    char selectfrom[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    char* token = (char*)calloc(1, TOKEN_LEN);
    int i;
    for(i = 0; i < TOKEN_LEN-1; i++) {
        token[i] = selectfrom[rand() % sizeof(selectfrom)];
    }
    token[TOKEN_LEN-1] = '\0';

    char* command;
    //test to see if the token is already in the DB
    asprintf(&command, "GET %s", token);
    char* response = runCommandStr(con, command);
    //if the token is already in use, make a new one
    if(response != NULL)
        token = get_db_token(con, username);
    free(response);
    //need to DELETE this token key when the user terminates connection
    //however in the current state there isn't a way for the program to detect this
    asprintf(&command, "SET %s %s", token, username);
    syslog(LOG_DEBUG, "Added token %s for user %s", token, username);
    freeReplyObject(runCommand(con, command));

    return token;
}

//accepts username and password
//checks to see if it is a correct password
//if so, returns auth token
//if user does not exist, creates user and returns auth token
char* hdb_authenticate(hdb_connection* con, const char* username, const char* password) {
    char* command;
    asprintf(&command, "HGET %s password", username);
    char* response = runCommandStr(con, command);
    if(response != NULL) {
        //check pw
        if(!strcmp(response, password)) {
            free(response);
            printf("Successfully logged in as %s\n", username);
            return get_db_token(con, username);
        }
        //bad pw :(
        else {
            free(response);
            printf("Authentication failed: Invalid password.\n");
            return NULL;
        }
    }
    else {
        free(response);
        //username DNE
        printf("Creating user %s with password %s\n", username, password);
        asprintf(&command, "HSET %s password %s", username, password);
        freeReplyObject(runCommand(con, command));
        return get_db_token(con, username);
    }
}

//verifies that passed auth token exists in db and returns username associated with it
//if DNE, return NULL (which runCommandStr will do anyway)
char* hdb_verify_token(hdb_connection* con, const char* token) {
    char* command;
    asprintf(&command, "GET %s", token);
    return runCommandStr(con, command);
}
