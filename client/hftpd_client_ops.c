#include "hftpd_client_ops.h"

msg* data_from_file(char* abs_path, uint32_t f_size, uint8_t seq_num, int* index) {
    //copy file into char array
    FILE* fp = fopen(abs_path, "r");
    char* buffer = (char*)calloc(1, f_size+1);
    fread(buffer, f_size, 1, fp);
    fclose(fp);

    //separate the char array into segments
    char* buffer_tosend = (char*)calloc(1, MSG_DATA_BUFFER_SIZE);
    if(f_size+1-*index < MSG_DATA_BUFFER_SIZE) {
        //compy f_size+1 - the amount already sent in index
        memcpy(buffer_tosend, buffer + *index, f_size+1-*index);
        *index += f_size+1-*index;
    }
    else {
        memcpy(buffer_tosend, buffer + *index, MSG_DATA_BUFFER_SIZE-1);
        f_size = MSG_DATA_BUFFER_SIZE-1;
        *index += MSG_DATA_BUFFER_SIZE-1;
    }
    free(buffer);
    //nullterm
//    uint_buffer[*index] = '\0';

    msg* final_msg = build_data_request(seq_num, strlen(buffer_tosend), buffer_tosend);
    free(buffer_tosend);
    return final_msg;
}

//the meat of the client. performs all sending of data to the server, as well as calling methods
//to build the various request types.
void client_operations(int sockfd, host* host, hfs_entry* files, char* auth_token, char* username, int num_files) {
    int seq_num = 0;

    hfs_entry* current_file = &files[0];
    int current_file_index = 1;
    while(current_file) {
        //get length of file in bytes
        FILE* fp = fopen(current_file->abs_path, "r");
        fseek(fp, 0L, SEEK_END);
        if(!fp) {
            syslog(LOG_ERR, "Error reading file %s, could not include in request.", current_file->abs_path);
            fclose(fp);
            continue;
        }

        uint32_t f_size = ftell(fp);
        fseek(fp, 0L, SEEK_SET);
        fclose(fp);

        msg* control_msg = build_control_init_request(seq_num, strlen(current_file->rel_path),
                                                 f_size, current_file->crc32, auth_token, current_file->rel_path);
        //something is wrong with network order here.
//        control_msg->length = htonl(control_msg->length);
        char* msg_type_str = get_msg_type(control_msg->buffer[0]);
        syslog(LOG_DEBUG, "Sending %s msg. Sequence # %d. Data size: %d",
               msg_type_str, control_msg->buffer[1], control_msg->length);
        free(msg_type_str);
        send_msg(sockfd, control_msg, host);

        int errcode = wait_for_ack(control_msg, sockfd, seq_num, host);
        //make sure no error!
        if(errcode) {
            syslog(LOG_EMERG, "Received error code from HFTP server. Likely bad auth token.");
            errx(EXIT_FAILURE, "Received error code from HFTP server. Likely bad auth token.");
        }
//        free(control_msg->buffer);
        free(control_msg);

        int index = 0, prev_index = 0, interval_counter = 0;
        //while file is still being sent
        while(index < f_size+1) {
            seq_num = !seq_num;
            //data request - index is out param to say where in the file has already been processed
            msg* data_msg = data_from_file(current_file->abs_path, f_size, seq_num, &index);
            msg_type_str = get_msg_type(data_msg->buffer[0]);
            //not necessarily whole file; check with index < f_size+1
            syslog(LOG_DEBUG, "Sending %s msg. Sequence # %d. Data size: %d",
               msg_type_str, data_msg->buffer[1], data_msg->length);
            free(msg_type_str);
            send_msg(sockfd, data_msg, host);
            //need error check here?
            wait_for_ack(data_msg, sockfd, seq_num, host);
//            free(data_msg->buffer);
            free(data_msg);
            interval_counter += index - prev_index;
            prev_index = index;
            if(interval_counter >= OUTPUT_INTERVAL) {
                interval_counter = 0;
                if(f_size != 0) {
                syslog(LOG_INFO, "Transferring file %s, %d of %d. %d bytes successfully transferred of %d, %.1lf%% complete.",
                       current_file->rel_path, current_file_index, num_files, index, f_size, ((double)index/(double)f_size)*100);
                }
            }
        }
        syslog(LOG_INFO, "Finished transferring %s, %d of %d.", current_file->rel_path, current_file_index, num_files);

        seq_num = !seq_num;
        current_file = current_file->next;
        current_file_index++;
    }

    msg_control* term_msg_control = (msg_control*)calloc(1, sizeof(msg_control));
    term_msg_control->type = TYPE_CONTROL_TERM;
    term_msg_control->seq  = seq_num;
    term_msg_control->length = sizeof(term_msg_control->type + term_msg_control->seq);

    msg* term_msg = (msg*)term_msg_control;

    send_msg(sockfd, term_msg, host);

    wait_for_ack(term_msg, sockfd, seq_num, host);

    free(term_msg_control);
}
