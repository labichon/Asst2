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

// Define thread nodes
typedef struct threadNode {
	pthread_t *thread;
	struct threadNode *next;
} threadNode;

// Define token nodes
typedef struct tokNode {
	int frequency;
	char* token;
	float prob;
	struct tokNode *nextHash;
	struct tokNode *nextLL;
} tokNode;

// Define file nodes
typedef struct fileNode {
	int numTokens;
	char *pathName;
	tokNode *sortedTokens;
	struct fileNode *next;
} fileNode;

// Define parameters to be passed into
// the void functions
typedef struct parameters {
	char *directname;
	fileNode **head;
	pthread_mutex_t *lock;
} parameters;

// Define Jensen-Shannon Distance Nodes
typedef struct JSDNode {
	double jsd; //Jenson Shannon Distance
	fileNode *file1;
	fileNode *file2;
} JSDNode;


/* joinThreads(threadNode*)
 * Input: threadNode pointer (*head)
 * Output: N/A
 * We interate through the thread linked list
 * in order to free and join threads
 */
void joinThreads(threadNode *head) {
	threadNode *temp;
	while (head != NULL) {
		pthread_join(*(head->thread), NULL);
		free(head->thread); // Free the pthreads
		temp = head;
		head = head->next;
		free(temp); // Free the nodes
	}
}


/* hash(char* , int)
 * Input: char* (key), length of key (keyLen)
 * Output: hashed value
 * Hashes the first 10 characters of the key
 */
int hash(char* key, int keyLen){
        int x = 0;
	int end = (keyLen < 10) ? keyLen : 10; // Make 10 a const
        for(int i = 0; i < end; i++) x += key[i];
        x = abs(x % HASHLEN);
        return x;
}


/* initHash(tokNode**)
 * Input: tokNode (HashMap**)
 * Output: N/A
 * Initializes hashmap
 */
void initHash(tokNode **HashMap){
        for(int i = 0; i < HASHLEN; i++){
                HashMap[i] = NULL;
        }
}


/* *searchHash(tokNode ** , char*, int)
 * Input: Hashmap (tokNode**) , token to insert (str), size of key (int)
 * Output: NULL if not found, node found otherwise
 * We search the Hash in order to find a key
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


/* *insertHash(tokNode**, char*, int)
 * Input: HashMap (tokNode**), token to insert (str), len (int)
 * Output: Pointer to node inserted or  NULL if already in list
 * We check if the token is in hashmap, and then create node
 * if it isn't
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

/* insertSortedLL(tokNode**, tokNode*)
 * Input: tokNode pointer (**head), tokNode (*toInsert)
 * Output: N/A
 * We move through LL in order to find the correct position
 * to insert the tokNode *toInsert
 */
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

/* jensonShannon(fileNode* , fileNode*)
 * Input: fileNode (*file1) , fileNode (*file2)
 * Output: Jenson-Shannon Distance value of the two files
 * Computes Jenson-Shannon Distance of two files
 */
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

/* *filehandle(void*)
 * Input: void (*args) - parameters struct
 * Output: NULL
 * File handling function for our pthread:
 * tokenizes files, sorts linked lists and inserts
 * the sorted linked lists into another linked list
 */
void *filehandle(void *args){
	
	// Open file and access path name
	char *pathName = ((parameters *)args)->directname;
	if (DEBUG) printf("File Path: %s\n", pathName);
	int fd = open(pathName, O_RDONLY);
	
	// Check if file could not be opened
	if (fd == -1) {
		perror(pathName);
		free(pathName);
		free(args);
		pthread_exit(NULL);
	}

	// Reset number of tokens
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
					
					// Var used - 1 is equal to strlen(token)
					// Insert into hashmap
					tokNode *newNode = insertHash(hashmap, token, used-1);
					
					if (newNode != NULL) {
						// Insert into sorted LL
						insertSortedLL(&sortedTokens, newNode);
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
	if (used > 0) { // Insert last token
		if (used == size) {
			// Check size and realloc if not enough
			size *= 2; // Multiply size by 2
			token = realloc(token, size);
		}
		token[used++] = '\0';
		// End of token:
		// Insert to DS and reset token list

		// Var used - 1 is equal to strlen(token)
		// Insert into hashmap
		tokNode *newNode = insertHash(hashmap, token, used-1);

		if (newNode != NULL) {
			// Insert into sorted LL
			insertSortedLL(&sortedTokens, newNode);
		}
		numTokens++;
	}
	close(fd);
	free(token);

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

/* *directhandle(void *args)
 * Input: void (*args) - parameters struct
 * Output: NULL
 * Directory handling function for our pthread:
 * creates pthreads for directories if found and
 * files if found, then joins the threads
 */
void *directhandle(void *args){

	// Create ease of use vars and open directory	
	char *dirName = ((parameters *)args)->directname;
	DIR* dir = opendir(dirName);
	struct dirent *dp;

	// Check if we can't open directory
	if (errno != 0) {
		perror(dirName);
		free(((parameters *)args)->directname); // Free passed pathname
		free(args); // Free passed args
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

		// Initialize parameters to be passed through
		// into pthreads via a struct
		parameters *arg = (parameters *)malloc(sizeof(parameters));
		arg->directname = pathName;
		arg->lock=((parameters *)args)->lock;
		arg->head=((parameters *)args)->head;

		if(dp->d_type == DT_REG){
			// Found a regular file
			if (DEBUG) printf("%s: Found regular file\n", pathName);

			pthread_create(thread, NULL, filehandle, arg);
			threadList = toInsert;
		} else if(dp->d_type == DT_DIR){
			// Found a directory
			if (DEBUG) printf("%s: Found directory\n", pathName);
		
			pthread_create(thread, NULL, directhandle, arg);
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

/* splitLL(fileNode* , fileNode** , fileNode**)
 * Input: fileNode (*head), fileNode (**left),  fileNode (**right)
 * Output: N/A
 * Splits the linked list for mergesort
 */
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


/* *merge(fileNode*, fileNode*)
 * Input: fileNode (*left) , fileNode (*right)
 * Output: Merged FileNode
 * Merges sublists recursively
 */
fileNode *merge(fileNode *left, fileNode *right) {
	// Base cases
	if (left == NULL) return right;
	else if (right == NULL) return left;
	
	// Recurisve cases
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

/* mergeSortLL(fileNode**)
 * Input: fileNode (**headRef)
 * Output: N/A
 * Merge sorts the linked list in a recursive manner
 */
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


/* sortCompareFunc(const void* , const void*)
 * Input: const void values (*val1) and (*val2)
 * Output: retval for qsort
 * Function that compares two elements in qsort
 */
int sortCompareFunc(const void *val1, const void *val2) {
	int sum1 = (*(JSDNode **)val1)->file1->numTokens + (*(JSDNode **)val1)->file2->numTokens;
	int sum2 = (*(JSDNode **)val2)->file1->numTokens + (*(JSDNode **)val2)->file2->numTokens;
	int retval;
	if (sum1 < sum2) retval = -1;
	else if (sum1 > sum2) retval = 1;
	else retval = 0;
	return retval;
}



/* printJSD(double)
 * Input: double (jsd)
 * Output: N/A 
 * Prints final JSD values with color
 */
void printJSD(double jsd) {
	if (jsd < 0.1) printf(RED);
	else if (jsd < 0.15) printf(YELLOW);
	else if (jsd < 0.2) printf(GREEN);
	else if (jsd < .25) printf(CYAN);
	else if (jsd < .3) printf(BLUE);
	else printf(WHITE);
	printf("%f", jsd);
	printf(RESET);
}


int main(int argc, char *argv[]){

	if (argc < 2) {
		exit(-1);
	}

	// Store directory from command line
	DIR* dir = opendir(argv[1]);

	// Check if dir is valid + accessible
	if (errno != 0) {
		perror(argv[1]);
		exit(-1);
	}

	// Check if directory exists
	if (dir) {

		// Create path name for directory
		size_t len = strlen(argv[1]);
		char *dirPath = (char *) malloc(len + 1);
		strcpy(dirPath, argv[1]);

		// Check if last char is '/' and if so, remove it
		if (dirPath[len-1] == '/') {
			dirPath[--len] = '\0';
		}
		
		// Create struct of parameters to pass through
		// into directhandle function
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

		// Check if there were files in directory
		if (*head_ref == NULL) {
			printf("ERROR: No files were parsed and added\n");
			exit(EXIT_FAILURE);
		}

		mergeSortLL(head_ref);
		
		// Access number of files
		int numFiles = 0;
		for (fileNode *file = *head_ref; file != NULL; file = file->next) numFiles++;

		// Check if there is a possible comparison
		if (numFiles < 2) {
			printf("ERROR: Not enough files to compare pairs (< 2 valid files)\n");
			exit(EXIT_FAILURE);
		}

		// Since files are compared with all others, there are 
		// summation(1 to n-1) pairs
		int totalPairs = (numFiles * (numFiles - 1)) / 2;
		JSDNode *filePairs[totalPairs];
		int fileNum = 0;

		// Compute all JSD
		for (fileNode *file1 = *head_ref; file1 != NULL; file1 = file1->next) {
			for(fileNode *file2 = file1->next; file2 != NULL; 
			    file2 = file2->next) 
			{
				JSDNode *toInsert = malloc(sizeof(JSDNode));
				toInsert->file1 = file1;
				toInsert->file2 = file2;
				toInsert->jsd = jensonShannon(file1, file2);
				filePairs[fileNum++] = toInsert;
			}
			// Free tokens
			tokNode *curr = file1->sortedTokens;
			while (curr != NULL) {
				tokNode *temp = curr;
				free(curr->token);
				curr = curr->nextLL;
				free(temp);
			}
		}

		qsort(filePairs, totalPairs, sizeof(JSDNode *), sortCompareFunc);
		
		// Print final values
		for (int i = 0; i < totalPairs; i++) {
			printJSD(filePairs[i]->jsd);
			printf(" \"%s\" and \"%s\"\n", filePairs[i]->file1->pathName,
			       filePairs[i]->file2->pathName); 
			free(filePairs[i]);
		}

		// Free memory
		fileNode *temp; 
		while (*head_ref != NULL) {
			temp = *head_ref;
			free(temp->pathName);
			*head_ref = (*head_ref)->next;
			free(temp);
		}

		closedir(dir);

	} else if (ENOENT == errno) {
		printf("error\n");
		return EXIT_FAILURE;
	}
	// If the directory does not exist, an error is returned


	return EXIT_SUCCESS;
}
