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
#define DEBUG 0
#endif

// Define colors
#define RED "\e[0;31m"
#define YELLOW "\e[0;33m"
#define GREEN "\e[0;32m"
#define CYAN "\e[0;36m"
#define BLUE "\e[0;34m"
#define WHITE "\e[0;37m"
#define RESET "\e[0m"

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
	unsigned int frequency;
	char* token;
	struct tokNode *nextHash;
	struct tokNode *nextLL;
} tokNode;

typedef struct fileNode {
	unsigned long numTokens;
	char *pathName;
	tokNode *sortedTokens;
	struct fileNode *next;
} fileNode;

//we need to pass a struct of info into the void function
//im sure as we move further along, more stuff will be added
typedef struct parameters {
	char *directname;
	fileNode **head;
	pthread_mutex_t *lock;
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
		// Iterate until the next node is null or greater than the current
		while (curr->nextLL != NULL && 
				strcmp(curr->nextLL->token, toInsert->token) < 0) {
			curr = curr->nextLL;
		}
		// Insert node after current node
		toInsert->nextLL = curr->nextLL;
		curr->nextLL = toInsert;
	}
}

void *filehandle(void *args){
	
	// Open file
	char *pathName = ((parameters *)args)->directname;
	if (DEBUG) printf("File Path: %s\n", pathName);
	int fd = open(pathName, O_RDONLY);
	if (fd == -1) {
		free(args);
		perror(pathName);
		pthread_exit(NULL);
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
	
	close(fd);
	free(token);

	if (DEBUG) {
		// Print linked list
		printf("HEAD -> ");
		for (tokNode *curr = sortedTokens; curr != NULL; curr=curr->nextLL) {
			printf("(\"%s\", %d) -> ", curr->token, curr->frequency);
		}
		printf("\nNum tokens: %lu\n", numTokens);
	}

	// Insert sorted LL into LL of files	
	fileNode **head = ((parameters *)args)->head;
	pthread_mutex_t *lock = ((parameters *)args)->lock;

	// Create fileNode to insert
	fileNode *toInsert = (fileNode *) malloc(sizeof(fileNode));
	toInsert->numTokens = numTokens;
	toInsert->pathName = pathName;
	toInsert->sortedTokens = sortedTokens;
	

	// Insert into front of LL
	pthread_mutex_lock(lock);
	toInsert->next = *head;
	*head = toInsert;
	pthread_mutex_unlock(lock);

	free(args);
	

	return NULL;
}


void *directhandle(void *args){

	// Create ease of use vars and open directory	
	char *dirName = ((parameters *)args)->directname;
	DIR* dir = opendir(dirName);
	struct dirent *dp;

	// Check if we can't open directory
	if (errno != 0) {
		free(((parameters *)args)->directname); // Free passed pathname
		free(args); // Free passed args
		perror(dirName);
		pthread_exit(NULL);
	}


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

		parameters *arg = (parameters *)malloc(sizeof(parameters));
		arg->directname = pathName;
		arg->lock=((parameters *)args)->lock;
		arg->head=((parameters *)args)->head;


		if(dp->d_type == DT_REG){
			// Found a regular file
			if (DEBUG) printf("%s: Found regular file\n", 
					   pathName);

			pthread_create(thread, NULL, filehandle, arg);
			threadList = toInsert;
		} else if(dp->d_type == DT_DIR){
			// Found a directory
			if (DEBUG) printf("%s: Found directory\n", pathName);
		
			// Create a pthread
			pthread_create(thread, NULL, directhandle, arg);
			// Add pthread to LL
			threadList = toInsert;
		} else {
			// Invalid type of file
			printf("%s: Invalid file type", pathName);
			free(arg);
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

	DIR* dir = opendir(argv[1]);
	// Check if dir is valid + accessible
	if (errno != 0) {
		perror(argv[1]);
		exit(-1);
	}

	//the assignment says to check a valid directory in the main,
	//but also in the directory-handling function
	//i put it the checking here for now
	if (dir) {
		size_t len = strlen(argv[1]);
		char *dirPath = (char *) malloc(len + 1);
		strcpy(dirPath, argv[1]);

		// Check if last char is '/' and if so, remove it
		if (dirPath[len-1] == '/') {
			dirPath[--len] = '\0';
		}

		parameters* arg = (parameters*) malloc(sizeof(struct parameters));
		
		// Create LL of LLs and mutex
		fileNode *head = NULL;
        	pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
        	arg->head = &head;
       		arg->lock = &lock;
        	
		// Add directory path to args
		arg->directname = dirPath;
		
		// Create reference to the head pointer
		fileNode **head_ref = &head;
		
		// Call the directory function
		directhandle(arg);

		// FIXME: Temporary debugging print statement
		for (fileNode *c = *head_ref; c != NULL; c = c->next) {
			printf("(Name: %s, Number of tokens: %lu) :", 
					c->pathName, c->numTokens);
                	for (tokNode *curr = c->sortedTokens; curr != NULL; curr=curr->nextLL) {
                        	printf("(\"%s\", %d) -> ", curr->token, curr->frequency);
                	}
                	printf("\n\n\n");
		}

		// TODO: Check if fileNode LL is still null
		// TODO: Math for all files

		// FIXME: Temporary debugging free statement
		fileNode *temp; 
		while (*head_ref != NULL) {
			temp = *head_ref;
			tokNode *curr = temp->sortedTokens;
			while (curr != NULL) {
				tokNode *temp = curr;
				free(curr->token);
				curr = curr->nextLL;
				free(temp);
			}
			free(temp->pathName);
			*head_ref = (*head_ref)->next;
			free(temp);
		}

		closedir(dir);
	} else if (ENOENT == errno) {
		printf("error\n");
		return EXIT_FAILURE;
	}
	
	return EXIT_SUCCESS;
}
