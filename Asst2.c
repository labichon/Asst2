#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

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
struct parameters {
	char* directname;
	
};


void *directhandle(void *holder){
	
	struct parameters *argument = holder;
	DIR* dir = opendir(argument->directname);
	struct dirent *dp;

	while((dp = readdir(dir))){	
		if(!strcmp(dp->d_name, ".")){
			continue;
		}
		if(!strcmp(dp->d_name, "..")){
			continue;
		}
		if(dp->d_type == DT_REG){
			//printf("aids3\n");
		}
	       	if(dp->d_type == DT_DIR){			
			struct parameters* dirargument = (struct parameters*) malloc(sizeof(struct parameters));
                	//need to find a way to put the name of this directory
			//into the parameters struct to pass to another pthread
			/*dirargument->directname = directoryname;
                	pthread_t direct;
                	pthread_create(&direct, NULL, directhandle, argument);
                	pthread_join(direct, &rval);*/
		}
		
	}
	closedir(dir);
	
	
}


int main(int argc, char *argv[]){

	char *directoryname = argv[1];
	DIR* dir = opendir(directoryname);
	void *rval; //return value

	//the assignment says to check a valid directory in the main,
	//but also in the directory-handling function
	//i put it the checking here for now
	if (dir) {
		struct parameters* argument = (struct parameters*) malloc(sizeof(struct parameters));
		argument->directname = directoryname;
		pthread_t direct;
		pthread_create(&direct, NULL, directhandle, argument);
		pthread_join(direct, &rval);
		printf("success\n");	
		closedir(dir);
	} else if (ENOENT == errno) {
		printf("error\n");
		return 0;
	}

	return 0;

}
