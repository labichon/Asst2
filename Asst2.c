#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>

struct list {
	char* id;
	int count; //maybe not necessary, just here for now
	struct list *next;
};

struct parameters {
	char* directname;
	
};


void *directhandle(void *arg){
	
	struct parameters *dirarg = arg;
	DIR* dir = opendir(dirarg->directname);
	struct dirent *dp;

	while((dp = readdir(dir))){
		printf("aids\n");
	}
	
}


int main(int argc, char *argv[]){

	char *directoryname = argv[1];
	DIR* dir = opendir(directoryname);
	void *rval; //return value

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
