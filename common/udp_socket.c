#include "../common/udp_socket.h"

//it would seem that this can be used for both server and client. so, later it will be moved to common code.
//client - node = "server_hostname", flags = 0
//server - node = NULL, flags = AI_PASSIVE
struct addrinfo* get_udp_sockaddr(const char* node, const char* port, int flags) {

    struct addrinfo hints;
    struct addrinfo* results;
    int retval;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = flags;

    retval = getaddrinfo(node, port, &hints, &results);

    if(retval) {
        errx(EXIT_FAILURE, "getaddrinfo error: %s\n", gai_strerror(retval));
    }

    return results;
}

struct addrinfo* get_tcp_sockaddr(const char* hostname, const char* port) {
    struct addrinfo hints;
    struct addrinfo* results;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int retval = getaddrinfo(NULL, port, &hints, &results);
    if (retval)
        errx(EXIT_FAILURE, "Couldn't get tcp socket: %s", gai_strerror(retval));

    return results;
}

int open_tcp_connection(struct addrinfo* addr_list) {
    struct addrinfo* addr;
    int sockfd;
    //try all addresses, stop if one works
    for (addr = addr_list; addr != NULL; addr = addr->ai_next)
    {
        //attemp open
        sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        //failed; move on
        if (sockfd == -1)
        continue;

        //success
        if (connect(sockfd, addr->ai_addr, addr->ai_addrlen) != -1)
            break;
    }

    freeaddrinfo(addr_list);
    if (!addr)
        err(EXIT_FAILURE, "%s", "Unable to connect to hmdp server, cannot proceed.");
    else
        return sockfd;
}

msg* receive_message(int sockfd, host* source) {
    msg* m = (msg*)malloc(sizeof(msg));

    source->addr_len = sizeof(source->addr);

    m->length = recvfrom(sockfd, m->buffer, sizeof(m->buffer),
                           0, (struct sockaddr*)&source->addr, &source->addr_len);

//    syslog(LOG_DEBUG, "Received type %d msg seq# %d", m->buffer[0], m->buffer[1]);

    if(m->length > 0) {
        inet_ntop(  source->addr.sin_family, &source->addr.sin_addr,
                    source->friendly_ip, sizeof(source->friendly_ip));
        //need to free at caller lvl
        return m;
    }
    else {
        //didn't receive anything
        free(m);
        return NULL;
    }
}

//returns the type of message associated with the msg type as a string
char* get_msg_type(int type) {
    char* msg_type;
    if(type == TYPE_CONTROL_INIT)
        asprintf(&msg_type, "%s", "Control, Inititalization");
    else if(type == TYPE_CONTROL_TERM)
        asprintf(&msg_type, "%s", "Control, Termination");
    else if(type == TYPE_DATA)
        asprintf(&msg_type, "%s", "Data");
    else if(type == TYPE_ACK)
        asprintf(&msg_type, "%s", "ACK");
    //if this happens, it is bad.
    else
        asprintf(&msg_type, "%s", "Error; not found");

    return msg_type;
}

int send_msg(int sockfd, msg* m, host* dest) {
    return sendto(sockfd, m->buffer, m->length,
                  0, (struct sockaddr*)&dest->addr, dest->addr_len);
}

