#include "server_socket.h"

//local functions
int bind_socket(struct addrinfo*);
int wait_for_connection(int);
void connection_loop(int, hdb_connection*);
char* handle_auth(char*, int, hdb_connection*);
char* handle_list(char*, int, hdb_connection*);

static bool term_requested = false;

//set up the server socket, and then call connection_loop to begin listening.
void hmdp_server_init(const char* port, hdb_connection* con) {
    struct addrinfo hints, *results;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int retval = getaddrinfo(NULL, port, &hints, &results);

    if(retval)
        errx(EXIT_FAILURE, "%s", gai_strerror(retval));

    //get sockfd from bind
    int sockfd = bind_socket(results);

    if(listen(sockfd, 25) == -1) {
        err(EXIT_FAILURE, "%s", "Unable to listen on socket");
    }

    syslog(LOG_INFO, "Server listening on port %s", port);

    while(!term_requested) {
        int connectionfd = wait_for_connection(sockfd);
        //loop handles closing
        connection_loop(connectionfd, con);
    }

    close(sockfd);
}

//bind the socket to the selected address
int bind_socket(struct addrinfo* results) {
    struct addrinfo* addr;
    int sockfd;
    for(addr = results; addr != NULL; addr = addr->ai_next) {
        sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        //could not create socket; move on
        if(sockfd == -1)
            continue;

        char yes = '1';
        //allow reuse of port if server is in TIME_WAIT state
        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, & yes, sizeof(int)) == -1)
            err(EXIT_FAILURE, "%s", "Unable to set socket option");


        if(bind(sockfd, addr->ai_addr, addr->ai_addrlen) == -1) {
            //binding failed; move on
            close(sockfd);
            continue;
        }
        else
            //success!
            break;
    }

    if(addr == NULL)
        err(EXIT_FAILURE, "%s", "Unable to bind a socket");

    freeaddrinfo(results);

    return sockfd;
}

//wait for a connection and print when one is received
int wait_for_connection(int sockfd) {
    struct sockaddr_in client_addr;
    unsigned addr_len = sizeof(struct sockaddr_in);
    int connectionfd = accept(sockfd, (struct sockaddr*)&client_addr, &addr_len);

    if(connectionfd == -1)
        err(EXIT_FAILURE, "%s", "Unable to accept connection");

    char ip[INET_ADDRSTRLEN];
    inet_ntop(client_addr.sin_family, &client_addr.sin_addr, ip, sizeof(ip));
    syslog(LOG_INFO, "Connection accepted from %s\n", ip);
    return connectionfd;
}

//the listening loop. handles all interaction with the client, including building appropriate responses
void connection_loop(int connectionfd, hdb_connection* con) {
    char buffer[BUFFER_SIZE];
    int bytes_read;
    do {
        bytes_read = recv(connectionfd, buffer, sizeof(buffer)-1, 0);

        if(bytes_read > 0) {
            //null-term stream
            buffer[bytes_read] = '\0';
            syslog(LOG_DEBUG, "Received %d bytes: \n%s\n", bytes_read, buffer);

            //get the request type, and call the responding method
            int index;
            char* response;
            request_types_e request = get_request_type(buffer, &index);
            if(request == AUTH) {
                response = handle_auth(buffer, index, con);
            }
            else if(request == LIST) {
                response = handle_list(buffer, index, con);
                //this next line would be taken out of a "real" server,
                //but after requesting files, for the purposes of this assignment, exit
                term_requested = true;
            }

            syslog(LOG_DEBUG, "Sending: %s\n", response);
            if(send(connectionfd, response, strlen(response), 0) == -1) {
                warn("Unable to send data to client");
                break;
            }
            free(response);
        }

    } while(bytes_read > 0 && !term_requested);

    close(connectionfd);
}

//respond to AUTH request
char* handle_auth(char* request, int index, hdb_connection* con) {
    char *username, *password;
    //extract username, pw
    int new_index = extract_kv(request, index, &username);
    extract_kv(request, new_index, &password);

    //generate a token
    char* token = hdb_authenticate(con, username, password);
    char* response;
    if(token == NULL) {
        syslog(LOG_INFO, "Rejected AUTH request from %s - Password: %s", username, password);
        response = build_command("401 Unauthorized", NULL, NULL);
    }
    else {
        syslog(LOG_INFO, "Accepted AUTH request from %s - password: %s", username, password);
        key_value_t kv = {"Token", token, NULL};
        response = build_command("200 Authentication Successful", &kv, NULL);
    }
    free(username);
    free(password);
    free(token);
    return response;
}

//respond to LIST request
char* handle_list(char* request, int index, hdb_connection* con) {
    //extract and verify token
    char* token;
    int new_index = extract_kv(request, index, &token);
    char* username = hdb_verify_token(con, token);
    free(token);

    char* response;
    if(username != NULL) {
        //302/204
        char* body;
        int body_length = extract_body(request, new_index, &body);
        //now we have the body stored in body
        //need to see if files are stored
        char* files_needed = calloc(1, body_length);
        long response_body_len = 0;

        //split the body up, delimited by newlines
        char* filename = strtok(body, "\n");
        do {
            //get checksum
            char* chksum = strtok(NULL, "\n");
            //get the stored checksum. will return NULL if the file doesn't exist, which will work.
            char* stored_chksum = hdb_file_checksum(con, username, filename);
            //need this file. add it to the response body
            if(stored_chksum == NULL || strcmp(chksum, stored_chksum) != 0) {
                //don't want a new line at the start
                if(response_body_len > 0) {
                    char* temp = files_needed;
                    asprintf(&temp, "%s\n%s", files_needed, filename);
                    free(files_needed);
                    asprintf(&files_needed, "%s", temp);
                    free(temp);
                }
                else {
                    char* temp = files_needed;
                    asprintf(&temp, "%s", filename);
                    free(files_needed);
                    asprintf(&files_needed, "%s", temp);
                    free(temp);
                }
                response_body_len += strlen(filename) + 1;
            }
            free(stored_chksum);
//            free(chksum);
        } while((filename = strtok(NULL, "\n")) != NULL);

        if(response_body_len > 0) {
            char len_str[32];
            //subtract 2 from length for the double newline
            sprintf(len_str, "%ld", response_body_len-2);
            key_value_t kv = {"Length", len_str, NULL};
            syslog(LOG_INFO, "Requesting files:");
            syslog(LOG_INFO, "%s", files_needed);
            response = build_command("302 Files Requested", &kv, files_needed);
        }
        else {
            response = build_command("204 No Files Requested", NULL, NULL);
            syslog(LOG_INFO, "No files requested.");
        }
        free(filename);
        free(files_needed);
        free(body);
    }
    else {
        //bad login. 401
        syslog(LOG_DEBUG, "Received invalid auth token %s", username);
        syslog(LOG_INFO, "Invalid authentication token");
        response = build_command("401 Unauthorized", NULL, NULL);
    }
    free(username);
    return response;
}

void term_handler(int signal) {
    syslog(LOG_INFO, "User terminated hmdb server");
    term_requested = true;
}
