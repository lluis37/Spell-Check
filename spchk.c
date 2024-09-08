#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <stdbool.h>
#include "spchk.h"  

#define INITIAL_DICT_SIZE 1024 // Initial size of dictionary which can later be adjusted dynmaically
#define WORD_LENGTH 46 // Maximum word length for any word in a dictionary file

int main(int argc, char *argv[]) {
    // Validate argument count
    if (argc < 3) {
        printf("Usage: %s <dictionary> <directory or text file>...\n", argv[0]);
        return 1;
    }

    // Load dictionary
    int dictSize = 0;
    char **dictionary = loadDictionary(argv[1], &dictSize);
    if (!dictionary) {
        fprintf(stderr, "Failed to load dictionary.\n");
        return 1;
    }
    // Initialize linked list head for storing file paths
    node *head = NULL;
    struct stat buf;

    // Process each command-line argument (file or directory) for spelling check
    for (int i = 2; i < argc; i++) {
        // Duplicate pathname for storage
        char *pathname = strdup(argv[i]);
        if (!pathname) {
            perror("Failed to duplicate pathname");
            continue;
        }

        // Check file status
        if (stat(pathname, &buf) < 0) {
            perror(pathname);
            free(pathname);
            continue;
        }

        // Determine if file or directory and handle accordingly
        if (S_ISREG(buf.st_mode)) {
            push(pathname, &head);
        } else if (S_ISDIR(buf.st_mode)) {
            dTraversal(pathname, &head);
            free(pathname);
        } else {
            fprintf(stderr, "%s is neither a regular file nor a directory.\n", pathname);
            free(pathname);
        }
    }

    // Perform spelling check on each file in the list
    node *current = head;
    while (current) {
        checkSpelling(dictionary, dictSize, current->s);
        current = current->next;
    }

    // Clean up: free dictionary and list
    freeDictionary(dictionary, dictSize);
    freeList(head);

    return 0;
}

char **loadDictionary(const char *path, int *dictSize) {
    // Attempt to open the dictionary file for reading
    FILE *file = fopen(path, "r");
    if (!file) {
        perror("Error opening dictionary"); // Print error if file cannot be opened
        return NULL;
    }

    // Allocate initial memory for the dictionary array
    char **dictionary = malloc(INITIAL_DICT_SIZE * sizeof(char *));
    if (!dictionary) {
        perror("Memory allocation failed"); // Print error if memory allocation fails
        fclose(file); // Close the file before returning
        return NULL;
    }

    // Buffer to store each word read from the file
    char word[WORD_LENGTH];
    int size = 0; // Current number of words loaded
    int capacity = INITIAL_DICT_SIZE; // Current capacity of the dictionary array

    // Read each word from the file until EOF
    while (fgets(word, sizeof(word), file)) {
        // Remove the newline character from the end of the word
        word[strcspn(word, "\n")] = 0;

        // If the dictionary is full, double its capacity
        if (size >= capacity) {
            capacity *= 2;
            char **newDict = realloc(dictionary, capacity * sizeof(char *));
            if (!newDict) {
                perror("Realloc failed"); // Print error if reallocation fails
                freeDictionary(dictionary, size); // Free allocated memory
                fclose(file); // Close the file before returning
                return NULL;
            }
            dictionary = newDict;
        }

        // Duplicate the word and add it to the dictionary
        dictionary[size] = strdup(word);
        if (!dictionary[size]) {
            perror("Strdup failed"); // Print error if strdup fails
            freeDictionary(dictionary, size); // Free allocated memory
            fclose(file); // Close the file before returning
            return NULL;
        }
        size++; // Increment the size of the dictionary
    }

    fclose(file); // Close the file after all words have been loaded
    *dictSize = size; // Store the final size of the dictionary
    return dictionary; // Return the pointer to the dictionary array
}

void freeDictionary(char **dictionary, int dictSize) {
    // Loop through each entry in the dictionary
    for (int i = 0; i < dictSize; i++) {
        // Free the memory allocated for the current word in the dictionary
        free(dictionary[i]);
    }
    free(dictionary);
}

int isNumber(const char *s) {
    if (*s == '-' || *s == '+') s++;  

    // Save the original position after possible sign
    const char *original = s;

    if (!*s) return 0;  // Empty string is not a number

    // Flag to check if the last character is a period
    int lastCharIsPeriod = 0;

    while (*s) {
        if (!isdigit((unsigned char)*s)) {
            // If we're at the last character and it's a period, set flag and break
            if (*(s + 1) == '\0' && *s == '.') {
                lastCharIsPeriod = 1;
                break;
            } else {
                return 0;  // Not a number if any non-digit character is encountered
            }
        }
        s++;
    }

    // Return true (1) if it's a number or if it's a number followed by a period
    // Ensure there's at least one digit before the period if lastCharIsPeriod is set
    return (s != original) && (lastCharIsPeriod || *s == '\0');
}

int isValidCapitalization(const char *word, const char *dictionaryWord) {
    // If the dictionary word starts with an uppercase letter, the input word must match exactly.
    if (isupper(dictionaryWord[0])) {
        return strcmp(word, dictionaryWord) == 0;
    }

    // For dictionary words not starting with an uppercase, check for all lowercase or all uppercase
    for (int i = 0; word[i] != '\0'; i++) {
        if (!isalpha(word[i])) continue; // Skip non-alphabetical characters

        if (islower(word[i])) { // If any character is lowercase, the whole word should be lowercase
            for (int j = i + 1; word[j] != '\0'; j++) {
                if (isupper(word[j])) return 0; // Found an uppercase after a lowercase
            }
            return 1; // The rest of the word is lowercase
        } else { // The word is starting with an uppercase letter
            for (int j = i + 1; word[j] != '\0'; j++) {
                if (islower(word[j])) return 0; // Found a lowercase after starting with uppercase
            }
            return 1; // The word is all uppercase
        }
    }

    return 0; 
}

int checkHyphenatedWord(char *word, char **dictionary, int dictSize) {
    char *token = strtok(word, "-");
    while (token) {
        if (!isWordInDictionary(dictionary, dictSize, token)) {
            return 0; // If any component is not in the dictionary, return false.
        }
        token = strtok(NULL, "-");
    }
    return 1; // All components are in the dictionary.
}

void stripPunctuationAndCheck(char *word, char **dictionary, int dictSize, const char *filePath, int lineNum, int start, int *errorFound) {
    // Starting point of the word within the line for error reporting
    int wordStart = start;
    // Pointer to iterate through characters in word
    char *ptr = word;
    
    // Remove leading punctuation
    while (*ptr && !isalnum(*ptr) && *ptr != '-') ptr++;
    start += ptr - word; // Update start position after skipping leading punctuation

    // Check if the word is empty or consists only of hyphens
    if (*ptr == '\0' || strspn(ptr, "-") == strlen(ptr) || isNumber(ptr)) {
        return; // Skip checking
    }
    
    // Container for the processed word (without leading/trailing punctuation)
    char processedWord[WORD_LENGTH] = {0};
    char *dest = processedWord;

    // Copy characters to processedWord, stopping at trailing punctuation or end of string
    int hyphenFound = 0; // Flag to track presence of hyphens
    while (*ptr && (isalnum(*ptr) || (*ptr == '-' && isalnum(*(ptr + 1))))) {
        if (*ptr == '-') {
            hyphenFound = 1; // Mark that a hyphen was found
        }
        *dest++ = *ptr++;
    }
    *dest = '\0'; // Ensure the new string is null-terminated

    // Check if the word (or each part of a hyphenated word) is in the dictionary
    int valid = 1; // Assume word is valid initially
    if (hyphenFound) {
        // Manual parsing of hyphenated words
        char *current = processedWord;
        char segment[WORD_LENGTH] = {0};
        while (*current) {
            char *segPtr = segment;
            // Extract segment up to the next hyphen or end of string
            while (*current && *current != '-') {
                *segPtr++ = *current++;
            }
            *segPtr = '\0'; // Null-terminate the segment

            if (!isWordInDictionary(dictionary, dictSize, segment)) {
                valid = 0; // Mark as invalid if any part is not in the dictionary
                break; // Exit loop if any segment is invalid
            }

            if (*current) current++; // Skip the hyphen
        }
    } else {
        valid = isWordInDictionary(dictionary, dictSize, processedWord);
    }

    // Report error if the word is invalid
    if (!valid) {
        printf("%s (%d,%d): %s\n", filePath, lineNum, wordStart + 1, word);
        *errorFound = 1;
    }
}

void checkSpelling(char **dictionary, int dictSize, const char *filePath) {
    // Announce which file is being opened for spell checking
    printf("Opening file: %s\n", filePath);

    // Attempt to open the file for reading
    FILE *file = fopen(filePath, "r");

    // If the file couldn't be opened, print an error message and exit the program
    if (!file) {
        perror("Error opening file to check");
        exit(EXIT_FAILURE);
    }

    // Buffer to hold each line of text from the file
    char line[1024];
    // Line number tracker, starting from 1 for the first line
    int lineNum = 1;
    // Indicator to track if any spelling error is found 
    int errorFound = 0;

    // Read the file line by line until the end is reached or the buffer is full
    while (fgets(line, sizeof(line), file)) {
        // Tokenize the current line by spaces, tabs, and new line characters to get individual words
        char *token = strtok(line, " \t\n");

        // Process each token (word) in the current line
        while (token) {
            // Strip punctuation from the word, check its spelling, and mark if an error is found
            stripPunctuationAndCheck(token, dictionary, dictSize, filePath, lineNum, token - line, &errorFound);
            // Get the next token from the current line
            token = strtok(NULL, " \t\n");
        }

        // Move to the next line in the file
        lineNum++;
    }

    // After checking the entire file, print a message if no spelling errors were found
    if (!errorFound) {
        printf("No spelling errors found in file: %s\n", filePath);
    }

    // Close the file to free up system resources
    fclose(file);
}


int binarySearch(char **dictionary, int low, int high, char *word) {
    while (low <= high) {
        int mid = low + (high - low) / 2;
        int res = strcasecmp(word, dictionary[mid]);

        if (res == 0) {
            return 1; // Word found
        } else if (res < 0) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    return 0; // Word not found
}

int isAllLowerCase(const char *s) {
    while (*s) {
        if (!islower(*s) && isalpha(*s)) return 0;
        s++;
    }
    return 1;
}

char* toLowerCase(const char* input) {
    char* lowercase = malloc(strlen(input) + 1); // +1 for the null terminator
    if (lowercase == NULL) {
        // Handle allocation failure if needed
        return NULL;
    }
    // Assume this loop converts the string to lowercase
    for (int i = 0; input[i]; i++) {
        lowercase[i] = tolower(input[i]);
    }
    lowercase[strlen(input)] = '\0'; // Don't forget the null terminator
    return lowercase;
}


int isAllUpperCase(char *word) {
    for (int i = 0; word[i] != '\0'; i++) {
        if (!isupper(word[i])) {
            return 0; 
        }
    }
    return 1; 
}

int isCapitalized(char *word) {
    if (isupper(word[0]) && word[1] != '\0') {
        for (int i = 1; word[i] != '\0'; i++) {
            if (isupper(word[i])) {
                return 0; // If any other character is uppercase, it's not just capitalized.
            }
        }
        return 1; // Only the first character is uppercase, and there is at least one more character.
    }
    return 0;
}

int isWordInDictionary(char **dictionary, int dictSize, char *word) {
    int exactMatchFound = 0;
    int caseInsensitiveMatchFound = 0;
    
    char *modifiedWord = strdup(word); // Duplicate word for manipulation.
    toLowerCase(modifiedWord + 1); // Lowercase all but the first character.
    

    for (int i = 0; i < dictSize; i++) {
        if (strcmp(dictionary[i], word) == 0) {
            // Exact match found.
            exactMatchFound = 1;
            break;
        }
        if (strcasecmp(dictionary[i], word) == 0) {
            // Case-insensitive match found.
            caseInsensitiveMatchFound = 1;
        }
        if (strcasecmp(dictionary[i], modifiedWord) == 0) {
            // Special case for capitalized words (e.g., beginning of sentences).
            caseInsensitiveMatchFound = 1;
        }
    }

    free(modifiedWord); 

    if (exactMatchFound || (caseInsensitiveMatchFound && (isAllUpperCase(word) || isCapitalized(word)))) {
        // If there's an exact match, or a case-insensitive match for all-uppercase words or words starting with a capital letter.
        return 1;
    }
    // If no conditions are met, the word is incorrect.
    return 0;
}

void push(char *pathname, node **head) {
    node *newNode = malloc(sizeof(node));
    if (!newNode) {
        perror("Failed to allocate memory for new node");
        free(pathname); // Free the allocated memory before returning
        return;
    }

    newNode->s = pathname;
    newNode->next = *head;
    *head = newNode;
}


void dTraversal(char *dName, node **head){
    DIR *directory = opendir(dName);
    if (directory == NULL) {
        return;
    }

    struct dirent *file;
    while ((file = readdir(directory)) != NULL) {
        // Make sure file does not being with "."
        if (file->d_name[0] == '.') {
            continue;
        }

        // Construct pathname for file
        char *path = malloc(strlen(dName) + strlen(file->d_name) + 2);
        memcpy(path, dName, strlen(dName));
        path[strlen(dName)] = '/';
        memcpy(path + strlen(dName) + 1, file->d_name, strlen(file->d_name));
        path[strlen(dName) + strlen(file->d_name) + 1] = '\0';
        // Construction complete

        struct stat buf;
        int r = stat(path, &buf);
        if (r < 0) {
            perror(path);
            continue;
        }
        // Check if file is a regular file or a directory
        char text[5];
        memcpy(text, path + strlen(dName) + strlen(file->d_name) - 3, 5);
        if (S_ISREG(buf.st_mode) && (!strcmp(text, ".txt"))) {
            push(path, head);
        } else if (S_ISDIR(buf.st_mode)) {
            dTraversal(path, head);
            free(path);
        } else {
            // Invalid file
            free(path);
        }
    }

    closedir(directory);
}

void freeList(node *head) {
    node *tmp;

    while (head) {
        tmp = head;
        head = head->next;
        free(tmp->s);
        free(tmp);
    }
}
