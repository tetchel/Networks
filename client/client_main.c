#include "client.h"
#include "client_socket.h"

//replaces ~ in a string with user's home directory (eg. ~/documents -> /home/tim/documents)
char* tilde_to_path(char* path) {
    //index of ~
    int index = strcspn(path, "~");
    //no tilde, return
    if(index == strlen(path))
        return path;

    //get home dir using linux api
    struct passwd *pw = getpwuid(getuid());
    char* homedir = pw->pw_dir;

    //characters before ~
    char before[index];
    memcpy(before, &path, index);
    before[index] = '\0';

    char* result;
    asprintf(&result, "%s%s%s", before, homedir, path + index+1);
    //no longer need the old path
    free(path);
    return result;
}

//generates a list command from given token and file list
char* build_list_command(char* token, char* list_of_files) {
    int list_len = strlen(list_of_files);
    //remove final newline
    list_of_files[list_len-1] = '\0';
    char list_len_str[8];
    sprintf(list_len_str, "%d", list_len);

    key_value_t kvs[2] = {{"Token", token, &kvs[1]}, {"Length", list_len_str, NULL}};

    char* command = build_command("LIST", kvs, list_of_files);
    return command;
}

//reads contents of file into char*
char* file_to_string(FILE* f) {
    char* buffer;
    if (f)
    {
      fseek (f, 0, SEEK_END);
      long length = ftell (f);
      fseek (f, 0, SEEK_SET);
      buffer = malloc (length+1);
      if (buffer)
      {
        fread (buffer, 1, length, f);
      }
      fclose (f);
      buffer[length] = '\0';
    }
    else
        syslog(LOG_EMERG, "Bad FILE* passed to file_to_string");

    return buffer;
}

//main function
int main(int argc, char* argv[]) {
	const char* hostname = NULL, *port = NULL;
	char* dirname = NULL, *username = NULL, *password = NULL;

    openlog("hooli_client", LOG_PERROR | LOG_PID | LOG_NDELAY, LOG_USER);
    setlogmask(LOG_UPTO(LOG_INFO));
    syslog(LOG_INFO, "Client started");

	while(1) {
		static struct option long_options[] = {
			{"verbose", no_argument, 0, 'v'},
			{"server", 	required_argument, 0, 's'},
			{"port", 	required_argument, 0, 'p'},
			{"dir", 	required_argument, 0, 'd'},
			{0,0,0,0}
		};
		int opt_index = 0;
		int c = getopt_long(argc, argv, "vs:p:d:", long_options, &opt_index);
		if(c == -1)
			break;

		switch(c) {
			case 'v':
				setlogmask(LOG_UPTO(LOG_DEBUG));
				break;
			case 's':
				hostname = optarg;
				break;
			case 'p':
			    //if 12x34 is passed, it will set port = 12 since sscanf stops
			    //if no number is passed it will keep port = 9000 which I think is OK
				port = optarg;
				break;
			case 'd':
				dirname = optarg;
				break;
			case '?':
				exit(EXIT_FAILURE);
				break;
		}
	}

    if(optind + 2 != argc) {
        printf("Usage: ./client -[options] --[options] username password\n");
        syslog(LOG_EMERG, "Usage: ./client -[options] --[options] username password\n");
        syslog(LOG_EMERG, "Wrong number of arguments passed.");
        exit(EXIT_FAILURE);
    }
	username = argv[optind];
	password = argv[optind+1];

	if(port == NULL)
        port = "5000";
	if(hostname == NULL)
		hostname = "127.0.0.1";
	if(dirname == NULL)
		asprintf(&dirname, "%s", "~/hooli/");
    //dirname must end in /
    else {
        if(dirname[strlen(dirname) - 1] != '/')
            asprintf(&dirname, "%s/", dirname);
    }

    dirname = tilde_to_path(dirname);

    DIR* root = opendir(dirname);
	//if it failed, we can't continue
	if(!root) {
		syslog(LOG_EMERG, "Failed to open %s, program will now exit. Make sure it exists and you have permissions.\n", dirname);
		exit(EXIT_FAILURE);
	}

    //prepare the client socket
	int sockfd = hmdp_client_init(hostname, port);
	//prepare an initial AUTH request
	key_value_t auth_kv[2] = {{"Username", username, &auth_kv[1]}, {"Password", password, NULL}};
	char* request = build_command("AUTH", auth_kv, NULL);

    //send AUTH, receive token
	char* token = send_message(sockfd, request);
    free(request);

    FILE* outfile = tmpfile();
	if(token != NULL) {
        //if AUTH accepted,
        if(!outfile) {
            printf("Failed to open outfile for writing, program will now exit.\n");
            syslog(LOG_EMERG, "Failed to open outfile for writing, program will now exit.\n");
            exit(EXIT_FAILURE);
        }
        int rootlen = strlen(dirname);
        //begin file tree traverse
        traverseDir(root, dirname, rootlen, outfile);
        //send LIST
        char* list_of_files = file_to_string(outfile);
        request = build_list_command(token, list_of_files);
        send_message(sockfd, request);
        free(request);
        free(list_of_files);
        free(token);
	}

//	close resources
    free(dirname);
	close(sockfd);
	closedir(root);
	closelog();

	return 0;
}
