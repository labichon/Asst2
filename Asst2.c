#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <ctype.h>


#define BUFSIZE 256

#ifndef DEBUG
#define DEBUG 0
#endif

// The ONLY reason there isn't a header file is because I'm not sure if they
// are allowed :(

typedef struct threadNode {
	pthread_t *thread;
	struct threadNode *next;
} threadNode;


//there should be a test directory too
//theres directory and a txt file in that
//directory

struct list {
	char* token;
	int prob;
	struct list *next;
};

//we need to pass a struct of info into the void function
//im sure as we move further along, more stuff will be added
typedef struct parameters {
	char* directname;
	// Insert shared data structure
} parameters;

// Join threads and free them
void joinThreads(threadNode *head) {
	// Iterate through thread LL to free and join threads
	threadNode *temp;
	while (head != NULL) {
		pthread_join(*(head->thread), NULL);
		free(head->thread); // Free the pthreads
		temp = head;
		head = head->next;
		free(temp); // Free the nodes
	}
}


//not understanding why open is stopping
//everything else from happening
void *filehandle(void *args){
	
	char* fileName = (char*)args;
	printf("%s\n", fileName);
	int fd = open("fileName", O_RDONLY); //everything after this doesn't print
	printf("aids\n"); //doesn't print
	if (fd < 0) {
		printf("error\n");
		exit(EXIT_FAILURE);
	} 
/*	
	int d, bytes;

	while (0 < (bytes = read(fd, &d, sizeof(int)))) {
        	printf("Read %d bytes: %x\n", bytes, d);
    	}
*/
	close(fd);
	printf("aids2\n"); //doesn't print
}


void *directhandle(void *args){

	// Create ease of use vars and open directory	
	char *dirName = ((parameters *)args)->directname;
	DIR* dir = opendir(dirName);
	struct dirent *dp;

	// Create LL of directory and file threads
	threadNode *threadList = NULL;

	// Read in each element inside the directory
	while((dp = readdir(dir))){

		if (DEBUG) printf("Reading \"%s\"\n", dp->d_name);

		if(!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")){
			// If we find a directory that is "." or ".." we skip
			// These correspond to the current and previous
			// directories
			if (DEBUG) printf("%s: Skipping iteration\n",
				          dp->d_name);
			continue;
		}
		
		// Declare the pthread and allocate space
		pthread_t *thread = malloc(sizeof(pthread_t));

		// Create thread node to insert
		threadNode *toInsert = malloc(sizeof(threadNode));
		toInsert->thread = thread;	
		toInsert->next = threadList;

		// Create new pathName including current path
		char *pathName = (char *) malloc(2 + strlen(dirName) + 
				  strlen(dp->d_name));
		strcpy(pathName, dirName);
		strcat(pathName, "/");
		strcat(pathName, dp->d_name);

		if(dp->d_type == DT_REG){
			// Found a regular file
			if (DEBUG) printf("%s: Found regular file\n", 
					   pathName);
			// temp free
			pthread_create(thread, NULL, filehandle, dp->d_name);
			free(pathName);
			free(toInsert->thread);
			free(toInsert);
		} else if(dp->d_type == DT_DIR){
			// Found a directory
			if (DEBUG) printf("%s: Found directory\n", pathName);
		
			// Create args to pass
			parameters *args = (parameters *)malloc(sizeof(parameters));
			args->directname = pathName;


			// Create a pthread
			pthread_create(thread, NULL, directhandle, (void *) args);
			// Add pthread to LL
			threadList = toInsert;
		} else {
			// Invalid type of file
			printf("lol wut");
			free(pathName);
			free(toInsert->thread);
			free(toInsert);
		}

	}
	closedir(dir);
	
	free(((parameters *)args)->directname); // Free parent's pathname
	free(args); // Free parent's created args

	
	// Iterate through thread LL to free and join threads
	joinThreads(threadList);

	return NULL;	
}

int main(int argc, char *argv[]){
	
	char *dirName = malloc(sizeof(strlen(argv[1])));
	strcpy(dirName, argv[1]);
	DIR* dir = opendir(dirName);
	void *rval; //return value
	
	// TODO: Check if dir is valid + accessible

	//the assignment says to check a valid directory in the main,
	//but also in the directory-handling function
	//i put it the checking here for now
	if (dir) {
		struct parameters* argument = (struct parameters*) malloc(sizeof(struct parameters));
		argument->directname = dirName;
		pthread_t direct;
		pthread_create(&direct, NULL, directhandle, argument);
		pthread_join(direct, &rval);
		printf("success\n");	
		closedir(dir);
	} else if (ENOENT == errno) {
		printf("error\n");
		return EXIT_FAILURE;
	}
	

	return EXIT_SUCCESS;

}
