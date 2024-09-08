#define _POSIX_C_SOURCE 200809L

// Use a linked list to manage all provided text files
typedef struct node_t {
    char *s;
    struct node_t *next;
} node;

// Function prototypes

/*
Responsible for loading the contents of a dictionary file specified by the path parameter into memory. 
It reads each word from the file, dynamically allocates memory for these words, and stores pointers 
to them in an array of character pointers (strings).
*/
char **loadDictionary(const char *path, int *dictSize);

/*
Checks the spelling of words in a text file against the words loaded into the dictionary. 
It reports any words that do not match any entry in the dictionary.
*/
void checkSpelling(char **dictionary, int dictSize, const char *filePath);

/*
Frees the memory allocated for the dictionary. 
This is called after the dictionary is no longer needed, to avoid memory leaks.
*/
void freeDictionary(char **dictionary, int dictSize);

/*
Determines whether a given word is present in the dictionary. 
It searches the dictionary for a matching word, ignoring case differences.
*/
int isWordInDictionary(char **dictionary, int dictSize, char *word);

/*
Pushes a new node containing a pathname onto the front of a linked list. 
This function is used to build a list of files to be processed for spelling checks.
*/
void push(char *pathname, node **head);

/*
Recursively traverses directories starting from 'dName', and adds all text files found 
to the linked list for processing. This function helps in handling directories provided 
as input, ensuring that all text files within are queued for spelling checks.
*/
void dTraversal(char *dName, node **head);

/*
Frees the memory allocated for the linked list. 
This function is called after all files in the list have been processed, to avoid memory leaks.
*/
void freeList(node *head);

/*
Checks if the given string represents a valid number. This is used to skip number strings 
when performing spelling checks, as numbers are not expected to be found in the dictionary.
*/
int isNumber(const char *s);

/*
Processes each word extracted from the text file, stripping leading and trailing punctuation, 
and then checks if the word exists in the dictionary. It also handles the reporting of the word's 
position in the text file if it is not found in the dictionary.
*/
void stripPunctuationAndCheck(char *word, char **dictionary, int dictSize, const char *filePath, int lineNum, int start, int *errorFound);
