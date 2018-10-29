#include "proxycache.h"

/*
 * README
 * Implementation of a forward cache for a proxy server. Creates
 * two lists and one hash table for LRU and LFU functionality.
 */

//////////

typedef struct Node {
    char *buffer;                /* Buffer for web object                */
    int buf_len;                 /* Size of buffer for web object        */
    time_t time;                 /* Time node was last accessed          */
    int frequency;               /* Frequency count of URL               */
    char url[MAX_URL_CHARS];     /* URL string 						     */
    struct Node *left;           /* Pointer to left node in linked list  */
    struct Node *right;          /* Pointer to right node in linked list */
} Node;

typedef struct List {
    int size;                    /* Size of list                         */
    int capacity;				 /* Capacity of list 					 */
    Node *head;                  /* Pointer to head node                 */
    Node *tail;                  /* Pointer to tail node                 */
} List;

//////////

typedef struct Hash_Node {       /* Node for hash table                   */
    int frequency;               /* Frequency count of URL                */
    time_t time;                 /* Time node was last accessed           */
    Node *LRU_element;           /* Pointer to node in LRU                */
    Node *LFU_element;           /* Pointer to node in LFU                */
} Hash_Node;

typedef struct Hash_Table {      /* Hash table                            */
    int size;                    /* Number of elements in the table       */
    int min_freq;                /* Minimum frequency in LFU cache        */
	int cache_size;				 /* Size of the cache in bytes            */
    Hash_Node *table[MAX_CACHE_ELEMENTS];  /* Array of Hash_Nodes         */
} Hash_Table;

//////////

static List LFU_cache;			 /* Least Frequently Used cache 		  */
static List LRU_cache;			 /* Least Recently Used cache 			  */
static Hash_Table hash_table;    /* Hash table 					 		  */

//////////

void constructCache() {
	LFU_cache.capacity = MAX_LFU_ELEMENTS;
	LRU_cache.capacity = MAX_LRU_ELEMENTS;
}

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

//////////

/* Remove a node from the linked list. Do not update the hash table */
/* values - this function is used for reordering. popCache is used  */
/* for permanent removal.										    */
static void removeForInsert(List cache, Node* node) {
    printf("\n*********REMOVING OBJECT FROM CACHE*********\n\n");

}

//////////

/* Pop the last element of the linked list. */
static void popCache(List cache) {
    printf("\n*********POPPING OBJECT FROM CACHE*********\n\n");
	
	int key;  			/* Key for the hash table 		  */
	Hash_Node *node;	/* Value returned from hash table */

	/* Hash incoming URL */
	key = hash(cache.tail->url);

	/* Get the value */
	node = hash_table.table[key];

	/* Update the hash table values */
	node->LFU_element = NULL;
	node->LRU_element = NULL;
	node->frequency = 0;

	/* Decrement the hash table size */
	hash_table.size--;

	/* Decrement the hash table size in bytes */
	hash_table.cache_size -= cache.tail->buf_len;

	/* Remove object from the cache */
	removeForInsert(cache, cache.tail);
}

//////////

/* Push a node onto the linked list. */
static void pushIntoCache(List cache, Node* node) {
    printf("\n*********PUSHING OBJECT INTO CACHE*********\n\n");

    /* Make space in the cache */
    if (cache.size == cache.capacity) {
    	popCache(cache);
    }

}

//////////

/* Add an object to the hash table, and either the LRU or LFU caches.  */
/* There should not be redundant entries between each cache, therefore */
/* each object is only added to one cache. 							   */
void addToCache(char *url, char *object, int object_size) {
    printf("\n*********ADDING OBJECT TO CACHE*********\n\n");

	/* Hash incoming URL */
	int key = hash(url);

    /* Check if there is room in the cache */
    if ((hash_table.cache_size - MAX_CACHE_SIZE) < object_size) {
    	fprintf(stderr, "Could not add cache entry due to lack of memory\n");
    	return;
    }

    /* Check if the object fits in the cache */
    if (object_size >= MAX_OBJECT_SIZE) {
    	fprintf(stderr, "Could not add cache entry due to the object size\n");
    	return;
    }

    /* Instantiate a hash table entry */
    Hash_Node *hash_node = Malloc(sizeof(struct Hash_Node));
    hash_node->frequency = 1;
    hash_node->time = time(NULL);
    hash_node->LFU_element = NULL;
    hash_node->LRU_element = NULL;

	hash_table.table[key] = hash_node;
	hash_table.cache_size += object_size;

    /* Instantiate a node */
    Node *node = Malloc(sizeof(struct Node));
    node->buffer = object;
    node->frequency = hash_node->frequency;
    node->time = hash_node->time;
    node->left = NULL;
    node->right = NULL;
    node->buf_len = object_size;
    strcpy(node->url, url);

    /* If there is room in LFU, add the node. */
    /* If added to LFU, don't add to LRU.     */
    if (LFU_cache.size < MAX_LFU_ELEMENTS) {
    	hash_node->LFU_element = node;
    	pushIntoCache(LFU_cache, node);
    	return;
    }

    /* Add to LFU if the frequency is greater than */
    /* the smallest frequency in the LFU cache.    */
    /* If added to LFU, don't add to LRU.     	   */
	if (LFU_cache.tail != NULL) {
		if (node->frequency > LFU_cache.tail->frequency) {
			hash_node->LFU_element = node;
    		pushIntoCache(LFU_cache, node);
			return;
		}
	}

    /* Else, add to LRU */
	hash_node->LRU_element = node;
	pushIntoCache(LRU_cache, node);

}

//////////

/* Return an object from the cache. Update the object's frequency    */
/* and access time within the cache. Reorder the cache linked lists. */
void getFromCache(char *url, char *object, int *object_size) {
    printf("\n*********GETTING OBJECT FROM CACHE*********\n\n");

	int key;  			/* Key for the hash table 		  */
	Hash_Node *node;	/* Value returned from hash table */

	/* Hash incoming URL */
	key = hash(url);

	/* Get the value */
	node = hash_table.table[key];

	/* Check if object is in cache */
	if (node == NULL || node->frequency == 0) {
		return;
	}

	/* Frequency is not 0, the node is in one or both caches */
	node->frequency++;
	node->time = time(NULL);

	/* Check if node is in LFU */
	if (node->LFU_element != NULL) {
		/* Get object */
		object = node->LFU_element->buffer;

		/* Update object's frequency and timestamp */
		node->LFU_element->frequency = node->frequency; 
		node->LFU_element->time      = node->time; 

		/* Reorder the cache */
		if (node->LFU_element != LFU_cache.head) {
			removeForInsert(LFU_cache, node->LFU_element);
			pushIntoCache(LFU_cache, node->LFU_element);
		}

		return;

	}

	/* Check if node is in LRU */
	if (node->LRU_element != NULL) {
		/* Get object */
		object = node->LRU_element->buffer;

		/* Update object's frequency and timestamp */
		node->LRU_element->frequency = node->frequency; 
		node->LRU_element->time      = node->time; 

		/* Check if node can go in LFU */
		if (LFU_cache.tail != NULL) {
			if (node->LRU_element->frequency > LFU_cache.tail->frequency) {
				removeForInsert(LRU_cache, node->LRU_element);
				pushIntoCache(LFU_cache, node->LRU_element);
				return;
			}
		}

		/* Reorder the cache */
		if (node->LRU_element != LRU_cache.head) {
			removeForInsert(LRU_cache, node->LRU_element);
			pushIntoCache(LRU_cache, node->LRU_element);
		}

	}

}

//////////
