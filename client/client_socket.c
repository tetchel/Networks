#include "client_socket.h"

int open_connection(struct addrinfo*);
//initializes client socket to the given hostname and port
int hmdp_client_init(const char* hostname, const char* port) {
    struct addrinfo hints, *results;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int retval = getaddrinfo(NULL, port, &hints, &results);
    if(retval)
        errx(EXIT_FAILURE, "%s", gai_strerror(retval));

    int sockfd = open_connection(results);
    syslog(LOG_DEBUG, "Client sending to %s:%s", hostname,port);
    return sockfd;
}

//opens a TCP connection to one of the given addrinfo
int open_connection(struct addrinfo* results) {
    struct addrinfo* addr;
    int sockfd;
    for(addr = results; addr != NULL; addr = addr->ai_next) {
        sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

        if(sockfd == -1)
           continue;

        if(connect(sockfd, addr->ai_addr, addr->ai_addrlen) != -1)
            break;
    }
    freeaddrinfo(results);

    if(addr == NULL)
        err(EXIT_FAILURE, "%s", "Unable to connect");
    else
        return sockfd;
}

//send a message, return any important data received
char* send_message(const int sockfd, const char* msg) {
    char* buffer = calloc(BUFFER_SIZE, 1);
    //try and send the message to server
    if(send(sockfd, msg, strlen(msg), 0) == -1)
        err(EXIT_FAILURE, "%s", "Unable to send message to server");

    syslog(LOG_DEBUG, "Sending:%s", msg);

    //read response from server
    int total_bytes_read = 0;
    char* end_of_header = NULL;
    while((end_of_header = strstr(buffer, "\n\n")) == NULL) {
        int bytes_read = recv(sockfd, &buffer[total_bytes_read], sizeof(buffer-1-total_bytes_read), 0);
        if(bytes_read == -1)
            err(EXIT_FAILURE, "%s", "Unable to read from server: 1");

        total_bytes_read += bytes_read;
    }

    //#body characters that have been read
    int num_extra_chars = strlen(end_of_header + 2);
    //header & key-values are now in buffer
    buffer[total_bytes_read] = '\0';
    int index;
    //get response type
    response_types_e response = get_response_type(buffer, &index);

    //need to read body if received FILES_REQ
    if(response == FILES_REQ) {
        char* length_str;
        extract_kv(buffer, index, &length_str);
        //overwrite null terminator
        int bytes_before_body = total_bytes_read;
        int length;
        sscanf(length_str, "%d", &length);
        free(length_str);
        //any body characters that have already been read are subtracted from length
        length -= num_extra_chars;

        buffer = realloc(buffer, BUFFER_SIZE + length);

        while((total_bytes_read-bytes_before_body) < length) {
            int bytes_read = recv(sockfd, &buffer[total_bytes_read], sizeof(buffer-1-total_bytes_read), 0);

            if(bytes_read == -1)
                err(EXIT_FAILURE, "%s", "Unable to read from server: 2");

            total_bytes_read += bytes_read;
        }
    }

    //null-terminate and log
    buffer[total_bytes_read] = '\0';
    syslog(LOG_DEBUG, "%d bytes received:", total_bytes_read);
    syslog(LOG_DEBUG, "%s", buffer);

    //we now have the full response stored in buffer.
    //process it appropriately
    if(response == AUTH_SUCC) {
        char* token;
        extract_kv(buffer, index, &token);

        syslog(LOG_DEBUG, "AUTH successful, received token %s", token);
        free(buffer);
        return token;
    }
    else if(response == AUTH_FAIL) {
        syslog(LOG_INFO, "Incorrect username or password.");
        free(buffer);
        return NULL;
    }
    else if(response == FILES_REQ) {
        char* body;
        extract_body(buffer, index, &body);
        syslog(LOG_INFO, "Received 302 list of requested files from server:");
        syslog(LOG_INFO, "%s", body);
        free(body);
    }
    else if(response == NO_FILES) {
        syslog(LOG_INFO, "Received 204 No Files Requested from server");
    }

    free(buffer);
    //must close the socket at caller level
    return "";
}
