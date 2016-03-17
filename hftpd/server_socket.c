#include "server_socket.h"

static bool term_requested = false;

//main server loop, connects to hdb at port port and performs all data receiving and processing.
void hftp_server(const char* redis_server, const char* port, char* dir, const char* timewait) {
    struct addrinfo* info_result = get_udp_sockaddr(NULL, port, AI_PASSIVE);

    int sockfd = bind_socket(info_result);

    //connect to hdb
    hdb_connection* db_connection = hdb_connect(redis_server);
    if(!db_connection) {
        syslog(LOG_EMERG, "Couldn't open hdb connection! Program cannot proceed.");
        errx(EXIT_FAILURE, "Couldn't open hdb connection! Program cannot proceed.");
    }
    else {
        syslog(LOG_INFO, "Connected to hdb server at %s:%s", redis_server, port);
    }

    msg* msg;
    //out parameter, filled by receive_message with client data
    host source;
    //next seq #
    uint8_t next_seq = 0;
    //self explanatory
    char* current_file, *current_checksum, *current_user;
    //used by data msg to determine % done
    uint32_t bytes_done, file_size;
    //used to detect when to output progress
    int interval_counter;

    while(!term_requested) {
        msg = receive_message(sockfd, &source);
        //if term is requested during receive_message, a nonsense msg is interpreted, so ignore it
        if(msg != NULL && !term_requested) {
            //buffer[0] holds type, [1] holds seq#
            if(msg->buffer[1] == next_seq) {
                //nullterm
                msg->buffer[msg->length] = '\0';
                //determine msg type
                uint8_t msg_type = msg->buffer[0];
                //returned by ack function
                int ack_status = 0;
                char* msg_type_str = get_msg_type(msg_type);
                syslog(LOG_DEBUG, "Received message of type %s. Sequence # %d. Data length %d.",
                        msg_type_str, msg->buffer[1], msg->length);
                free(msg_type_str);

                if(msg_type == TYPE_CONTROL_INIT) {
                    //init msg
                    msg_control* msg_c = (msg_control*)msg;
                    //get auth token
                    char auth_token[AUTH_TOKEN_LEN+1];
                    memcpy(auth_token, msg_c->auth_token, AUTH_TOKEN_LEN);
                    auth_token[AUTH_TOKEN_LEN] = '\0';
                    //get current user so we know where to store files
                    bool errflag = false;
                    if(!current_user) {
                        current_user = hdb_verify_token(db_connection, auth_token);
                        //bad auth token or db_con
                        if(!current_user)
                            errflag = true;
                        //append username/ to dir (it already ends in /)
                        char* tmp;
                        asprintf(&tmp, "%s%s/", dir, current_user);
                        //causing segfault
//                        free(dir);
                        asprintf(&dir, "%s", tmp);
                        free(tmp);
                        free(current_user);
                    }
                    //give an error if current_user is still null, means db_connection or auth_token is bad
                    ack_status = ack(msg_c->seq, sockfd, &source, errflag);
                    //current_file = filename given by msg_c
                    //causing segfault
//                    if(current_file)
//                        free(current_file);
                    asprintf(&current_file, "%s", msg_c->buffer);
                    //to host order
                    msg_c->checksum = ntohl(msg_c->checksum);
                    asprintf(&current_checksum, "%X", msg_c->checksum);
                    //if the file already exists, delete it
                    char* abs_path;
                    asprintf(&abs_path, "%s%s", dir, current_file);
                    if(access(abs_path, F_OK) != -1) {
                        int remove_retval = remove(abs_path);
                        if(remove_retval)
                            syslog(LOG_ERR, "Couldn't delete old file %s!", abs_path);
                        else
                            syslog(LOG_DEBUG, "Deleted old file %s", abs_path);
                    }
                    free(abs_path);
                    //reset % done
                    bytes_done = 0;
                    file_size = ntohl(msg_c->file_size);
                    interval_counter = 0;
                }
                else if(msg_type == TYPE_CONTROL_TERM) {
                    //term msg
                    msg_control* msg_c = (msg_control*)msg;
                    ack_status = ack(msg_c->seq, sockfd, &source, false);
                    bool status = time_wait(next_seq, sockfd, &source, timewait);
                    if(status)
                        //reset sequence number for new connections after TIME_WAIT
                        //since it will be negated below, set to 1, then it will be reset to 0.
                        next_seq = 1;
                }
                else if(msg_type == TYPE_DATA) {
                    //data msg
                    msg_data* msg_d = (msg_data*)msg;
                    ack_status = ack(msg_d->seq, sockfd, &source, false);
                    //to host order
                    msg_d->data_len =  ntohs(msg_d->data_len);
                    //append the data to file
                    int retval = append_to_file(dir, current_file, msg_d->buffer, msg_d->data_len);
                    bytes_done += msg_d->data_len;
                    interval_counter += msg_d->data_len;
                    if(interval_counter >= OUTPUT_INTERVAL || file_size < OUTPUT_INTERVAL) {
                        //output transfer status
                        if(file_size != 0) {
                            syslog(LOG_INFO, "Receiving %s. %d bytes successfully transferred of %d, %.1lf%% complete.",
                                        current_file, bytes_done, file_size, ((double)bytes_done/(double)file_size)*100);
                        }
                        interval_counter = 0;
                    }

                    //retval == 1 indicates finished transfer
                    if(retval) {
                        syslog(LOG_INFO, "Finished receiving %s", current_file);
                        //update the file in redis.
                        hdb_record record;
                        record.checksum = current_checksum;
                        record.filename = current_file;
                        record.username = current_user;
                        hdb_store_file(db_connection, &record);
                    }
                }
                else {
                    //error, invalid msg type (should not happen)
                    syslog(LOG_ERR, "Server received invalid message type %d", msg_type);
                }
                //should not happen
                if(ack_status == -1)
                    syslog(LOG_ERR, "Failed to send ACK for seq #%d", msg->buffer[1]);
                else
                    next_seq = !next_seq;
            }
        }

        free(msg);
    }
    //Ctrl+C pressed
    if(db_connection)
        hdb_disconnect(db_connection);
//    if(current_file)
//        free(current_file);
//    if(current_user)
//        free(current_user);
    close(sockfd);
}

//sends an ACK message to the client with sequence number seq_num
int ack(uint8_t seq_num, int sockfd, host* dest, bool error) {
    syslog(LOG_DEBUG, "Sending ACK %d. Error %d.", seq_num, error);
    msg_response* ack_msg = malloc(sizeof(msg_response));
    ack_msg->length = sizeof(msg_response);
    ack_msg->type   = TYPE_ACK;
    ack_msg->seq    = seq_num;
    ack_msg->err_code = error;
    int retval = send_msg(sockfd, (msg*)ack_msg, dest);
    free(ack_msg);
    return retval;
}

//writes the data in buffer to dir/rel_path
//returns 0 if all goes well
int append_to_file(char* dir, char* rel_path, uint8_t* buffer, uint16_t buff_len) {
    char* abs_path;
    asprintf(&abs_path, "%s%s", dir, rel_path);
    //get index of last / in rel_path to see if directories must be created
    char* lastSlash = NULL;
    for(char* p = rel_path + strlen(rel_path); p >= rel_path; p--) {
        if(*p == '/') {
            lastSlash = p;
        }
    }
    //put the new directories into dir
    if(lastSlash) {
        char* dirs_in_relpath = malloc(sizeof(rel_path));
        memcpy(dirs_in_relpath, rel_path, lastSlash - rel_path);
        dirs_in_relpath[lastSlash - rel_path] = '\0';
        asprintf(&dir, "%s%s", dir, dirs_in_relpath);
    }

    //call mkdir
    char* mkdir_command;
    asprintf(&mkdir_command, "mkdir -p %s", dir);
    int retval = system(mkdir_command);
    if(retval)
        syslog(LOG_ERR, "Couldn't create directory for hftp files %s", dir);
    free(mkdir_command);

    FILE* fout = fopen(abs_path, "a+");
    //append the data to the file
    if(!fout) {
        syslog(LOG_ERR, "Null file pointer when writing to %s, aborting write", abs_path);
        fclose(fout);
        return -1;
    }
    fwrite(buffer, buff_len, 1, fout);
    fclose(fout);
    free(abs_path);
    if(buff_len < MSG_DATA_BUFFER_SIZE)
        //last transmission for this file
        return 1;
    else
        //more data to be sent
        return 0;
}

//TIME_WAIT state, waits timewait seconds while ACKing the client's termination msg
//before exiting the program.
//return value indicates whether to reset and await new connection, or continue with current session
//continue with current session iff a request is received besides type 2 during TIME_WAIT
bool time_wait(uint8_t seq_num, int sockfd, host* dest, const char* timewait) {
    struct pollfd fd = {
        .fd = sockfd,
        .events = POLLIN
    };

    int duration = 10;
    sscanf(timewait, "%d", &duration);
    syslog(LOG_DEBUG, "TIME_WAIT state");

    for(int i = 0; i < duration && !term_requested; i++) {
        int poll_status = poll(&fd, 1, 1000);
        if(i % 5 == 0) {
            syslog(LOG_DEBUG, "TIME_WAITing... %ds remaining", duration-i);
        }
        if(poll_status == 1 && fd.revents == POLLIN) {
            msg* message = receive_message(sockfd, dest);
            if(message->buffer[0] == TYPE_CONTROL_TERM) {
                //successful termination
                ack(seq_num, sockfd, dest, false);
                return true;
            }
            else {
                //received a non-term message! go back into regular state, waiting on all msg types.
                return false;
            }
        }
    }
    syslog(LOG_INFO, "Finished TIME_WAIT. Awaiting new connections...");
    return true;
}

//bind the socket to an address in results
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

//return if term is requested. used by server_main
bool get_term_status() {
    return term_requested;
}

//called when ctrl+c is pressed, exits the server loop.
void term_handler(int sig) {
    syslog(LOG_INFO, "Term requested");
    term_requested = true;
}
