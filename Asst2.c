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
	int frequency;
	char* token;
	float prob;
	struct tokNode *nextHash;
	struct tokNode *nextLL;
} tokNode;

typedef struct fileNode {
	int numTokens;
	char *pathName;
	tokNode *sortedTokens;
	struct fileNode *next;
} fileNode;

typedef struct meanNode {
	char* token;
	float prob;
	struct meanNode *next;
} meanNode;

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

// Compute Jenson-Shannon Distance of two files
double jensonShannon(fileNode *file1, fileNode *file2) {
	
	// Assume fileNode pointers given are not NULL and
	// have fields initialized
	if (file1 == NULL || file2 == NULL) {
		printf("ERROR: jensonShannon fileNodes assumed not NULL\n");
		return -1;
	}
	
	// Create kld sums (Kullbeck-Leibler Divergence)
	double kld1 = 0, kld2 = 0;

	tokNode *toks1 = file1->sortedTokens;
	tokNode *toks2 = file2->sortedTokens;
	
	int comparison;

	// Loop until both lists of tokens are done
	while(toks1 != NULL || toks2 != NULL) {
		// Compare the tokens to see which to are valid
		// -1: Smallest token only present in left tokList
		// 0: Smallest token present in both tokLists
		// 1: Smallest token only present in right tokList
		if (toks1 == NULL) comparison = 1;
		else if (toks2 == NULL) comparison = -1;
		else comparison = strcmp(toks1->token, toks2->token);
		
		// Initialize probability distributions to 0
		double probDist1 = 0, probDist2 = 0, meanProb;

		// Calculate probDist values and iterate lists appropriately
		if (comparison <= 0) {
			// Token present in first tokList
			probDist1 = (double)(toks1->frequency) / (file1->numTokens);
			toks1 = toks1->nextLL;
		}
		if (comparison >= 0) {
			// Token present in second tokList
			probDist2 = (double)(toks2->frequency) / (file2->numTokens);
			toks2 = toks2->nextLL;
		}

		meanProb = (probDist1 + probDist2) / 2;

		// Add to respective klds if necessary
		if (comparison <= 0) kld1 += probDist1 * log10(probDist1 / meanProb);
		if (comparison >= 0) kld2 += probDist2 * log10(probDist2 / meanProb);
	}

	if (DEBUG) printf("Computed KLD1 as %f and KLD2 as %f\n", kld1, kld2);

	return (kld1 + kld2) / 2;
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

	int numTokens = 0;

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
		printf("\nNum tokens: %d\n", numTokens);
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

void splitLL(fileNode *head, fileNode **left, fileNode **right) {
	// Head is guaranteed to not be NULL
	
	fileNode *fast = head->next; // Start at 2nd node so we don't pass mid
	fileNode *slow = head;

	// Node fast travels at twice the speed of slow
	// When fast reaches the end, slow is at the middle
	while (fast != NULL && fast->next != NULL) {
		slow = slow->next;
		fast = fast->next->next;
	}

	// Slow is at or 1 before mid
	*left = head;
	*right = slow->next;
	slow->next = NULL;
}

fileNode *merge(fileNode *left, fileNode *right) {
	// Recursive merge
	// Base cases
	if (left == NULL) return right;
	else if (right == NULL) return left;
	
	fileNode *ret;
	if (left->numTokens < right->numTokens) {
		left->next = merge(left->next, right);
		ret = left;
	} else {
		right->next = merge(left, right->next);
		ret = right;
	}
	return ret;
}

void mergeSortLL(fileNode **headRef) {

	// Base Case: Length 0 or 1 - already sorted	
	if (*headRef == NULL || (*headRef)->next == NULL) return;
	
	fileNode *left, *right;
	splitLL(*headRef, &left, &right);

	// Sort sublists then merge	
	mergeSortLL(&left);
	mergeSortLL(&right);
	
	*headRef = merge(left, right); 
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

		if (*head_ref == NULL) {
			//TODO: Fix this error statement
			printf("ERROR: Nothing was added\n");
			exit(-1);
		}

		mergeSortLL(head_ref);
			
		// FIXME: Temporary debugging print statement
		for (fileNode *c = *head_ref; c != NULL; c = c->next) {
			printf("(Name: %s, Number of tokens: %d) :", 
					c->pathName, c->numTokens);
                	for (tokNode *curr = c->sortedTokens; curr != NULL; curr=curr->nextLL) {
                        	printf("(\"%s\", %d) -> ", curr->token, curr->frequency);
                	}
                	printf("\n\n\n");
		}

		// TODO: Math for all files
		for (fileNode *file1 = *head_ref; file1 != NULL; file1 = file1->next) {
			for(fileNode *file2 = file1->next; file2 != NULL; file2 = file2->next) {
				printf("%f: \"%s\" and \"%s\"\n", 
						jensonShannon(file1, file2), file1->pathName,
						file2->pathName);
			}
		}

		// Temporary test function (need to implement for all files)
		printf("JS: %f\n", jensonShannon(*head_ref, (*head_ref)->next));

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
