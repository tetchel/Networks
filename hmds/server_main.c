#include "server_socket.h"
#include <time.h>

//main function - processes parameters and initializes the server listening for requests
int main(int argc, char* argv[]) {
    srand(time(NULL));
	const char* hostname = NULL, *port = NULL;

    openlog("hooli_server", LOG_PERROR | LOG_PID | LOG_NDELAY, LOG_USER);
    setlogmask(LOG_UPTO(LOG_INFO));
    syslog(LOG_DEBUG, "Server started");

    //getopt loop
	while(1) {
		static struct option long_options[] = {
			{"verbose", no_argument, 0, 'v'},
			{"redis", 	required_argument, 0, 'r'},
			{"port", 	required_argument, 0, 'p'},
			{0,0,0,0}
		};
		int opt_index = 0;
		int c = getopt_long(argc, argv, "vr:p:", long_options, &opt_index);
		if(c == -1)
			break;

		switch(c) {
			case 'v':
			    setlogmask(LOG_UPTO(LOG_DEBUG));
				break;
			case 'r':
				hostname = optarg;
				break;
			case 'p':
				port = optarg;
				break;
			case '?':
				exit(EXIT_FAILURE);
				break;
		}
	}

    //default values
	if(port == NULL)
        port = "9000";
	if(hostname == NULL)
		hostname = "127.0.0.1";

    syslog(LOG_DEBUG, "Connecting to hdb at %s", hostname);

    //sigterm handler
    struct sigaction action;
    action.sa_handler = term_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT, &action, NULL);

    //connect to the hdb server
    hdb_connection* con = hdb_connect(hostname);
    //initialize the server socket.
    hmdp_server_init(port, con);

    hdb_disconnect(con);
    closelog();
}
