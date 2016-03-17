#include "client.h"

//recursive directory tree algorithm
//root - directory currently being traversed
//path - string to which each directory is appended each recursive call, to give the current directory path
//rootlen - length of root folder name, so it can be easily excluded from output
//outfile - the file the contents of the filetree is written to
void traverseDir(DIR* root, char* path, int rootlen, FILE* outfile) {
    syslog(LOG_INFO, "Scanning %s", path);
	struct dirent* currentItem;
	while((currentItem = readdir(root)) != NULL) {
		//name of current file
		char* currentName = currentItem->d_name;
		//ignore . and ..
		if(!(strcmp(currentName, ".") == 0 || strcmp(currentName, "..") == 0)) {
			//see if current item is a subdirectory
            char* newpath;
            asprintf(&newpath, "%s%s/", path, currentName);
			// printf("newpath: %s\n", newpath);
			//check if currentItem is a directory
			if(currentItem->d_type == DT_DIR) {
				//recursively call on subdirectory
				DIR* dir = opendir(newpath);
				if(dir != NULL)
					traverseDir(dir, newpath, rootlen, outfile);
				else
					syslog(LOG_ERR, "Failed to open directory %s! Ensure you have permissions.\n", newpath);

				//close it after recursive call
				closedir(dir);
			}
			//if readdir returned null, it's a file, so append it to the file
			//directory names are not printed by themselves, only the files within are printed
			else {
				//add the filename to the path
                char* fullname;
				asprintf(&fullname, "%s%s", path, currentName);
				uLong checksum = computeChecksum(fullname);
				//record the filename (with path) and checksum to the file
				//add rootlen to remove the first directory from output
				syslog(LOG_INFO, "* %s(%lX)", fullname + rootlen, checksum);
				fprintf(outfile, "%s\n%lX\n", fullname + rootlen, checksum);
				free(fullname);
			}
			free(newpath);
		}
	}
	syslog(LOG_DEBUG, "Finished scanning %s", path);
}

//returns the checksum of the given file
uLong computeChecksum(char* filename) {
	FILE* fileptr = fopen(filename, "rb");			//open for binary reading
	fseek(fileptr, 0, SEEK_END);					//go to end of file
	long length = ftell(fileptr);					//length = difference between beginning and end of file
	rewind(fileptr);								//go back to beginning of file
	Bytef* buffer = malloc((length+1));				//allocate space for length+1 (\0) characters
	fread(buffer, length, 1, fileptr);				//read the file into the bytef array
	fclose(fileptr);

	uLong crc = crc32(0L, Z_NULL, 0);				//intialize before crc call according to zlib doc
	crc = crc32(crc, buffer, length);				//get the crc and return it

	free(buffer);
	return crc;
}
