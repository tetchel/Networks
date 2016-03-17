#include "server_socket.h"

#include <getopt.h>
#include <signal.h>

int main(int argc, char* argv[]) {

	const char* redis_server = NULL, *port = NULL, *timewait = NULL;
	char* dir = NULL;

    openlog("hftp_server", LOG_PERROR | LOG_PID | LOG_NDELAY, LOG_USER);
    setlogmask(LOG_UPTO(LOG_INFO));
    syslog(LOG_DEBUG, "HFTP Server started");

    //getopt loop
	while(1) {
		static struct option long_options[] = {
			{"verbose", no_argument, 0, 'v'},
			{"redis", 	required_argument, 0, 'r'},
			{"port", 	required_argument, 0, 'p'},
			{"dir", 	required_argument, 0, 'd'},
			{"timewait", required_argument, 0, 't'},
			{0,0,0,0}
		};
		int opt_index = 0;
		int c = getopt_long(argc, argv, "vr:p:d:t:", long_options, &opt_index);
		if(c == -1)
			break;

		switch(c) {
			case 'v':
			    //maybe there is a better way to do this so that stdout does not need to match syslog.
			    setlogmask(LOG_UPTO(LOG_DEBUG));
				break;
			case 'r':
				redis_server = optarg;
				break;
			case 'p':
				port = optarg;
				break;
            case 'd':
                dir = optarg;
                break;
            case 't':
                timewait = optarg;
                break;
			case '?':
				exit(EXIT_FAILURE);
				break;
		}
	}

    //default values
	if(!port)
        port = "9000";
	if(!redis_server)
		redis_server = "127.0.0.1";
    if(!dir)
        dir = "/tmp/hftpd/";
    else {
        //dir must end in a /
        if(dir[strlen(dir) - 1] != '/')
            asprintf(&dir, "%s/", dir);
    }
    if(!timewait)
        timewait = "10";

    syslog(LOG_DEBUG, "HFTP started at %s:%s, dir=%s, timewait=%s", redis_server, port, dir, timewait);

    //sigterm handler
    struct sigaction action;
    action.sa_handler = term_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT, &action, NULL);

    //start up the server, and go until it quits.
//    while(!get_term_status())
        hftp_server(redis_server, port, dir, timewait);

    closelog();
}
