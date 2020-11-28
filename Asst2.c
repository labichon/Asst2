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
#define INITIAL_TOKSIZE 16
#define HASHLEN 128

#ifndef DEBUG
#define DEBUG 1
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

typedef struct tokNode {
	char* token;
	int frequency;
	struct tokNode *nextHash;
	struct tokNode *nextLL;
} tokNode;

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

//function to hash our values
//First 10 chars
int hash(char* key, int keyLen){
        int x = 0;
	int end = (keyLen < 10) ? keyLen : 10; // Make 10 a const
        for(int i = 0; i < end; i++) x += key[i];
        x = abs(x % HASHLEN);
        return x;
}

//initialize HashMap
void initHash(tokNode **HashMap){
        for(int i = 0; i < HASHLEN; i++){
                HashMap[i] = NULL;
        }
}

/* HashMap Search
 * Input: Hashmap (tokNode**) , token to insert (str), size of key (int)
 * Output: NULL if not found, node found otherwise
*/
tokNode *searchHash(tokNode **HashMap, char *token, int len){
        int i = hash(token, len);
        if(HashMap[i] != NULL) {
                for (tokNode *curr = HashMap[i]; curr != NULL; curr = curr->nextHash){
			if (!strcmp(token, curr->token)) return curr;
                }
        }
        return NULL;
}

/* Hashmap insert
 * Input: HashMap (tokNode**), token to insert (str), len (int)
 * Output: Pointer to node inserted or  NULL if already in list
 * Note: 
*/
tokNode *insertHash(tokNode **HashMap, char *token, int len){
        int i = hash(token, len);

	// Check if token already in HashMap
	tokNode *found = searchHash(HashMap, token, len);
	if (found != NULL) {
		(found->frequency)++;
		return NULL;
	}

	// Create the new node
	tokNode *newNode = (tokNode*) malloc(sizeof(tokNode));
	
	// Copy the token into the node
	newNode->token = (char *) malloc(len+1);
	strcpy(newNode->token, token);
	newNode->frequency = 1;

	// Insert node
        newNode->nextHash = HashMap[i];
        HashMap[i] = newNode;
	return HashMap[i];
}

void insertSortedLL(tokNode **head, tokNode *toInsert) {
	tokNode *curr = *head;
	// Insert to front of LL (LL empty or node comes before head)
	if (curr == NULL || strcmp(toInsert->token, curr->token) < 0) {
		toInsert->nextLL = curr;
		*head = toInsert;
	} else { // Insert to middle or end of LL
		// Iterate to node before we want to insert
		while (curr->nextLL != NULL && strcmp(curr->token, toInsert->token) < 0) {
			curr = curr->nextLL;
		}
		// Insert node after current node
		toInsert->nextLL = curr->nextLL;
		curr->nextLL = toInsert;
	}

}

void *filehandle(void *args){
	
	// Open file
	char* fileName = (char*)args; // FIXME: We will have more args later
	int fd = open(fileName, O_RDONLY);
	if (fd == -1) {
		perror(fileName);
		exit(EXIT_FAILURE);
	}

	unsigned long numTokens = 0;

	// Declare the buffer elements
	char buf[BUFSIZE];
	int bytes;
	int size = INITIAL_TOKSIZE;

	// Create arraylist and insert+reset booleans
	char *token = malloc(size);
	int used = 0, insert = 0, reset = 0;


	// Create HashMap
	tokNode *hashmap[HASHLEN];
	initHash(hashmap);

	// Create LinkedList (sorted)
	tokNode *sortedTokens = NULL;

	while ((bytes = read(fd, buf, BUFSIZE)) > 0) {
		// Read in up to BUFSIZE bytes
		for (int pos = 0; pos < bytes; pos++) { // Read chars individually
			// Convert any alphabetic chars to lowercase
			char curr = isalpha(buf[pos]) ? tolower(buf[pos]) : buf[pos];
			
			// Check type of character
			if (isspace(curr) && used > 0) {
				// Whitespace char that terminates a token:
				// Insert the null terminator
				curr = '\0';
				insert = 1;
				reset = 1;
			} else if (isalpha(curr) || curr == '-') insert = 1;
			
			// If char is not a whitespace terminator, alphabetic, 
			// or '-' char, ignore it

			// Insert a char if needed
			if (insert) {
				// Insert char into arraylist
				if (used == size) {
					// Check size and realloc if not enough
					size *= 2; // Multiply size by 2
					token = realloc(token, size);
				}
				token[used++] = curr;
				if (reset) {
					// End of token:
					// Insert to DS and reset token list
					if (DEBUG) printf("Inserting token: %s\n", token);
					
					

					// Var used - 1 is equal to strlen(token)
					// Insert into hashmap
					tokNode *newNode = insertHash(hashmap, token, used-1);
					
					if (newNode != NULL) {
						// Insert into sorted LL
						insertSortedLL(&sortedTokens, newNode);
						if (DEBUG) printf("Inserted (\"%s\", %d)\n", 
							newNode->token, newNode->frequency);	
					}

					numTokens++;
					// reset the token ArrayList
					size = INITIAL_TOKSIZE;
					token = realloc(token, size);
					used = 0;
					reset = 0;
				}
				insert = 0;
			}
		}
	}

	free(token);

	if (DEBUG) printf("Num tokens: %lu\n", numTokens);
	// TODO: Insert sorted LL into LL of files
	
	if (DEBUG) {
		// Print linked list
		printf("HEAD -> ");
		for (tokNode *curr = sortedTokens; curr != NULL; curr=curr->nextLL) {
			printf("(\"%s\", %d) -> ", curr->token, curr->frequency);
		}
		printf("\n");
	
	}

	return NULL;
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
	
	char* fileName = argv[1];
	filehandle(fileName);
	
	/*
	// Read in and copy the directory name
	size_t len = strlen(argv[1]);
	// Check if last char is '/' and if so, remove it
	if (argv[1][len-1] == '/') {
		argv[1][len-1] = '\0';
		len--;
	}
	char *dirName = (char *) malloc(len + 1);
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
	*/
	return EXIT_SUCCESS;

}
