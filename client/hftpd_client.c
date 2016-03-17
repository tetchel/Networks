#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <getopt.h>
#include <pwd.h>

#include <hmdp.h>
#include <hfs.h>
#include <errno.h>

#include "hftpd_client_socket.h"
#include "hftpd_client_ops.h"

//replaces ~ in a string with user's home directory (eg. ~/documents -> /home/tim/documents)
char* tilde_to_path(char* path) {
    //index of ~
    unsigned index = strcspn(path, "~");
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

//deletes an item from the linked list of hfs_entries
void delete_hfs_entry(hfs_entry** head, char* rel_path) {
    hfs_entry* tmp = *head;
    hfs_entry* prev = NULL;

    while(strcmp(tmp->rel_path, rel_path) && tmp->next) {
        prev = tmp;
        tmp = tmp->next;
    }

    if(!strcmp(tmp->rel_path, rel_path)) {
        if(prev)
            prev->next = tmp->next;
        else
            *head = tmp->next;
        free(tmp);
    }
}

//main function. performs all metadata operations, then calls client_operations for hftp.
int main(int argc, char* argv[]) {
	const char* hmdp_server = NULL, *hmdp_port = NULL, *fserver = NULL, *fport = NULL;
	char* dirname = NULL, *username = NULL, *password = NULL;

    openlog("hftpd_client", LOG_PERROR | LOG_PID | LOG_NDELAY, LOG_USER);
    setlogmask(LOG_UPTO(LOG_INFO));
    syslog(LOG_INFO, "Client started");

	while(1) {
		static struct option long_options[] = {
			{"verbose", no_argument, 0, 'v'},
			{"server", 	required_argument, 0, 's'},
			{"port", 	required_argument, 0, 'p'},
			{"dir", 	required_argument, 0, 'd'},
			{"fserver",	required_argument, 0, 'f'},
			{"fport", 	required_argument, 0, 'o'},
			{0,0,0,0}
		};
		int opt_index = 0;
		int c = getopt_long(argc, argv, "vs:p:d:f:o:", long_options, &opt_index);
		if(c == -1)
			break;

		switch(c) {
			case 'v':
				setlogmask(LOG_UPTO(LOG_DEBUG));
				break;
			case 's':
				hmdp_server = optarg;
				break;
			case 'p':
				hmdp_port = optarg;
				break;
			case 'd':
				dirname = optarg;
				break;
            case 'f':
                fserver = optarg;
                break;
            case 'o':
                fport = optarg;
                break;
			case '?':
				exit(EXIT_FAILURE);
				break;
		}
	}

    //ensure required args were given
    if(optind + 2 != argc) {
        printf("Usage: ./client -[options] --[options] username password\n");
        syslog(LOG_EMERG, "Usage: ./client -[options] --[options] username password\n");
        syslog(LOG_EMERG, "Wrong number of arguments passed.");
        exit(EXIT_FAILURE);
    }
	username = argv[optind];
	password = argv[optind+1];

    //default values
	if(hmdp_port == NULL)
        hmdp_port = "5000";
	if(hmdp_server == NULL)
		hmdp_server = "127.0.0.1";
    if(fserver == NULL)
        fserver = "127.0.0.1";
    if(fport == NULL)
        fport = "10000";

	if(dirname == NULL)
		asprintf(&dirname, "%s", "~/hooli/");
        //dirname must end in /
    else {
        if(dirname[strlen(dirname) - 1] != '/')
            asprintf(&dirname, "%s/", dirname);
    }

    //correct ~ in dirname to /home/user
    dirname = tilde_to_path(dirname);

    syslog(LOG_DEBUG, "Client sending to hftp server at %s:%s, hmdp server at %s:%s", fserver, fport, hmdp_server, hmdp_port);
    syslog(LOG_INFO, "Scanning file tree...");
    //get hfs files
    errno = 0;
    hfs_entry* files = hfs_get_files(dirname);

//    open connection to hmds and get auth token
    struct hmdp_request* auth_request = hmdp_create_auth_request(username, password);
    //get sockfd for hmds
    struct addrinfo* tcp_addrinfo = get_tcp_sockaddr(hmdp_server, hmdp_port);
    int hmds_sockfd = open_tcp_connection(tcp_addrinfo);
    //AUTH request
    int response_code = hmdp_send_request(auth_request, hmds_sockfd);

    //AUTH response
    struct hmdp_response* auth_response = hmdp_read_response(hmds_sockfd);
    if(response_code || !auth_response)
        errx(EXIT_FAILURE, "Error receiving auth token from hmds. The application cannot proceed.");

    //save the auth_token
    char* auth_token;
    char* tmp = hmdp_header_get(auth_response->headers, "Token");
    asprintf(&auth_token, "%s", tmp);
//    free(tmp);
//    auth_token[AUTH_TOKEN_LEN] = '\0';
    syslog(LOG_INFO, "Received auth token %s", auth_token);

    //finished authentication
    if(auth_request)
        hmdp_free_request(auth_request);
    if(auth_response)
        hmdp_free_response(auth_response);

    //build a list of files for the LIST request
    char* list_request_body;
    hfs_entry* file = &files[0];
    asprintf(&list_request_body,"%s\n%X", file->rel_path, file->crc32);
    file = file->next;
    while(file) {
        char* tmp;
        asprintf(&tmp,"%s\n%s\n%X", list_request_body, file->rel_path, file->crc32);
        free(list_request_body);
        asprintf(&list_request_body, "%s", tmp);
        free(tmp);
        file = file->next;
    }

    //send the list request and process response
    struct hmdp_request* list_request = hmdp_create_list_request(auth_token, list_request_body);
    int list_response_code = hmdp_send_request(list_request, hmds_sockfd);
    struct hmdp_response* list_response = hmdp_read_response(hmds_sockfd);
    if(list_response_code || !list_response)
        errx(EXIT_FAILURE, "Error receiving auth token from hmds. The application cannot proceed.");

    if(list_response->code == 401)
        errx(EXIT_FAILURE, "Received unauthorized from hmds. The application cannot proceed.");

    //get the list of files as a string
    char* list_of_files;
    char* tmp2 = list_response->body;
    asprintf(&list_of_files, "%s", tmp2);
    free(tmp2);

    syslog(LOG_INFO, "Received file list from hmds: %s", list_of_files);

    //one of these two lines is causing heap corruption when run in the VM.
    //memleak is preferable to segfault... :(
//    if(list_request)
//        hmdp_free_request(list_request);
//    if(list_response)
//        hmdp_free_response(list_response);

    //remove unncessary files from files
    file = &files[0];
    int num_files = 0;
    while(file) {
        if(!strstr(list_of_files, file->rel_path)) {
            delete_hfs_entry(&files, file->rel_path);
        }
        else {
            num_files++;
        }
        file = file->next;
    }

    //socket binding operations
    host* host = malloc(sizeof(host));
    int sockfd = bind_socket(fserver, fport, host);

    //main part of the program; communicates with hftps
    client_operations(sockfd, host, files, auth_token, username, num_files);

    //all done! clean up
    close(sockfd);

    //this is supposed to free the LL returned from hfs_get_files,
    //but it doesn't work and causes mem error in VM
//    file = &files[0];
//    while(file) {
//        free(file->abs_path);
//        hfs_entry* tmp = file;
//        file = file->next;
//        free(tmp);
//    }
//    free(files);
    free(list_request_body);
    free(list_of_files);
    free(auth_token);
    //causing invalid next size in VM
//    free(host);
    free(dirname);
	closelog();
	return 0;
}
