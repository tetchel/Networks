#include "command_processor.h"

//returns a pointer to the string from the first instance of c
//returns null if not found
char* read_until(char* input, char c) {
    char buffer[BUFFER_SIZE];
    int i;
    //look for c in input
    for(i = 0; i < strlen(input); i++) {
        //copy into result
        buffer[i] = input[i];

        if(input[i] == c) {
            //terminate the string, removing the found character
            buffer[i] = '\0';
            char* result;
            asprintf(&result, "%s", buffer);
            return result;
        }
    }
    //won't happen unless a request/response is malformatted
    return NULL;
}

//return number of characters until c
//-1 if not found (doesn't happen)
int read_until_len(char* input, char c) {
    int i;
    //look for c in input
    for(i = 0; i < strlen(input); i++) {
        if(input[i] == c) {
            return i;
        }
    }
    return -1;
}

//get request type from first lien of message
request_types_e get_request_type(char* command, int* index) {
    char* firstline = read_until(command, '\n');
    *index = strlen(firstline) + 1;
    request_types_e type = ERR;
    if(strstr(firstline, "AUTH") != NULL)
        type = AUTH;
    if(strstr(firstline, "LIST") != NULL)
        type = LIST;

    if(type == ERR) {
        syslog(LOG_EMERG, "Invalid request passed to get_request_type: %s", firstline);
        err(EXIT_FAILURE, "Invalid request passed to get_request_type: %s", firstline);
    }

    free(firstline);

    return type;
}

//get response type from first lien of message
response_types_e get_response_type(char* command, int* index) {
    char* firstline = read_until(command, '\n');
    *index = strlen(firstline) + 1;
    response_types_e type = ERR2;
    if(strstr(firstline, "200") != NULL)
        type = AUTH_SUCC;
    if(strstr(firstline, "204") != NULL)
        type = NO_FILES;
    if(strstr(firstline, "302") != NULL)
        type = FILES_REQ;
    if(strstr(firstline, "401") != NULL)
        type = AUTH_FAIL;

    if(type == ERR2) {
        syslog(LOG_EMERG, "Invalid request passed to get_response_type: %s", firstline);
        err(EXIT_FAILURE, "Invalid request passed to get_response_type: %s", firstline);
    }

    free(firstline);

    return type;
}

//extracts value from a key-value pair in a message of the form \nKey:Value\n
//returns index of the end of the extracted kv (for easy sequential calls)
int extract_kv(char* message, int start, char** value) {
    //same size as server/client receive buffers
    char buffer[BUFFER_SIZE];
    //end of message index
    int final_index = strlen(message)+1-start ;
    //truncate the beginning of the string (index < start)
    memcpy(buffer, &message[start], final_index);
    buffer[final_index] = '\0';

    //get length of key
    int length = read_until_len(buffer, ':');
    int buff_len = strlen(buffer);
    //remove the key
    memcpy(buffer, &message[start+length+1], buff_len);
    //get the value and asprintf it into value
    char* result = read_until(buffer, '\n');
    asprintf(value, "%s", result);
    int result_len = strlen(result);
    free(result);
    //return the index of the start of message AFTER the part which has been extracted.
    //length of value + length of key + index of start + newline and colon
    return result_len + length + start + 2;
}

//extract the body from a message, given the index of its length field
long extract_body(char* message, int start, char** body) {
    char* length_str;
    int body_start = extract_kv(message, start, &length_str) + 1;
    //convert length to a number
    long length;
    sscanf(length_str, "%ld", &length);
    //copy body of msg into body
    asprintf(body, "%s", message + body_start);
    free(length_str);
    return length;
}

//firstline must have status or command
//key-values are key:value fields. passed as linked list
//these turned out to be less useful than I thought, but would work well for longer k:v lists
//body is optional body data - pass null pointer for none
char* build_command(char* firstline, key_value_t* key_values, char* body) {
    char* command;
    asprintf(&command, "%s\n", firstline);
    //used to temporarily store strings being combined, so that the old string can be freed
    char* temp;

    //process key value fields
    if(key_values != NULL) {
        key_value_t* kv = key_values;
        temp = command;
        asprintf(&temp, "%s%s:%s\n", command, kv->key, kv->value);
        free(command);
        asprintf(&command, "%s", temp);
        free(temp);
        while((kv = kv->next) != NULL) {
            temp = command;
            asprintf(&temp, "%s%s:%s\n", command, kv->key, kv->value);
            free(command);
            asprintf(&command, "%s", temp);
            free(temp);
        }
    }

    //kv-terminating newline
    temp = command;
    asprintf(&temp, "%s\n", command);
    free(command);
    asprintf(&command, "%s", temp);
    free(temp);
    if(body != NULL) {
        temp = command;
        asprintf(&temp, "%s%s", command, body);
        free(command);
        asprintf(&command, "%s", temp);
        free(temp);
    }
    //no newline at end of body
    return command;
}
