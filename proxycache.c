#include "proxycache.h"
#include <string.h>

/*
 * README
 * Implementation of a forward cache for a proxy server. Creates
 * two queues and one hash table for LRU and LFU functionality.
 */

//////////

typedef struct Node {
    char *buffer;                /* Buffer for web object                */
    time_t time;                /* Time URL was added to cache           */
    int frequency;               /* Frequency count of URL               */
    struct Node *left;           /* Pointer to left node in linked list  */
    struct Node *right;          /* Pointer to right node in linked list */
} Node;

typedef struct Queue {
    int size;                    /* Size of queue                        */
    Node *head;                  /* Pointer to head node                 */
    Node *tail;                  /* Pointer to tail node                 */
} Queue;

//////////

typedef struct Hash_Node {       /* Node for hash table                   */
    int frequency;               /* Frequency count of URL                */
    Node *LRU_element;           /* Pointer to node in LRU                */
    Node *LFU_element;           /* Pointer to node in LFU                */
} Hash_Node;

typedef struct Hash_Table {      /* Hash table                            */
    int size;                    /* Number of elements in the table       */
    Hash_Node table[MAX_CACHE_ELEMENTS];  /* Array of Hash_Nodes          */
} Hash_Table;

//////////

static Queue LFU_cache;
static Queue LRU_cache;
static Hash_Table hash_table;
static int cache_size = 0;

//////////

/* dbj2 hash function for strings. Adopted from: */
/* http://www.cse.yorku.ca/~oz/hash.html 		 */
static int hash(char *url) {
	unsigned long hash = 5381;
	int c;

	while ((c = *url++))
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

	return (int)(hash % MAX_CACHE_ELEMENTS);
}

static void removeFromCache(Queue cache, Node* node) {
	printf("temp\n");
}

static void enqueueCache(Queue cache, Node* node) {
	printf("temp\n");
}

static void dequeueCache(Queue cache, Node* node) {
	printf("temp\n");
}

//////////

void addToCache(char *url, char *object, int object_size) {
    printf("\n*********ADDING OBJECT TO CACHE*********\n\n");

	int key;			/* Key for the hash table 		  */
	Hash_Node value;	/* Value returned from hash table */

	/* Hash incoming URL */
	key = hash(url);

	/* Get the value */
	value = hash_table.table[key];

	// if not in lfu, and freq > lfu.freq, add to lfu. check if size == capacity

}

//////////

char* getFromCache(char *url) {
    printf("\n*********GETTING OBJECT FROM CACHE*********\n\n");

    int in_LFU = 0;     /* T/F if object is in LFU cache  */
	int key;  			/* Key for the hash table 		  */
	Hash_Node value;	/* Value returned from hash table */
	char *object;       /* Object stored in cache         */

	/* Hash incoming URL */
	key = hash(url);

	/* Get the value */
	value = hash_table.table[key];

	/* Object is not in either cache */
	if (value.frequency == 0) {
		return NULL;
	}

	/* Frequency is not 0, the value is in one or both caches */
	value.frequency++;

	/* Check if value is in LFU */
	if (value.LFU_element != NULL) {
		value.LFU_element->frequency++;

		/* Get object */
		object = value.LFU_element->buffer;
		in_LFU = 1;

		/* Reorder the cache */
		if (value.LFU_element != LFU_cache.head) {
			removeFromCache(LFU_cache, value.LFU_element);
			enqueueCache(LFU_cache, value.LFU_element);
		}

	}

	/* Check if value is in LRU */
	if (value.LRU_element != NULL) {
		value.LRU_element->time = time(NULL);

		/* Get object */
		if (!in_LFU) {
			object = value.LRU_element->buffer;
		}

		/* Reorder the cache */
		if (value.LRU_element != LRU_cache.head) {
			removeFromCache(LRU_cache, value.LRU_element);
			enqueueCache(LRU_cache, value.LRU_element);
		}

	}

	return object;

}

//////////
