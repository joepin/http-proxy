#include "proxycache.h"
#include <string.h>

/*
 * README
 * Implementation of a forward cache for a proxy server. Creates
 * two queues and one hash table for LRU and LFU functionality.
 */

//////////

typedef struct LFU_Node {        /* Least Frequenctly Used node          */
    char *buffer;                /* Buffer for web object                */
    int frequency;               /* Frequency count of URL               */
    struct LFU_Node *left;       /* Pointer to left node in linked list  */
    struct LFU_Node *right;      /* Pointer to right node in linked list */
} LFU_Node;

typedef struct LFU_Queue {       /* Least Frequently Used queue          */
    int size;                    /* Size of queue (<= 3)                 */
    LFU_Node *head;              /* Pointer to head node                 */
    LFU_Node *tail;              /* Pointer to tail node                 */
    int min_frequency;           /* Minimum frequency of nodes in queue  */
} LFU_Queue;

//////////

typedef struct LRU_Node {        /* Least Recently Used node              */
    char *buffer;                /* Buffer for web object                 */
    time_t *time;                /* Time URL was added to cache           */
    struct LRU_Node *left;       /* Pointer to left node in linked list   */
    struct LRU_Node *right;      /* Pointer to right node in linked list  */
} LRU_Node;

typedef struct LRU_Queue {       /* Least Recently Used queue             */
    int size;                    /* Size of queue (<= 997)                */
    LRU_Node *head;              /* Pointer to head node                  */
    LRU_Node *tail;              /* Pointer to tail node                  */
} LRU_Queue;

//////////

typedef struct Hash_Node {       /* Node for hash table                   */
    int frequency;               /* Frequency count of URL                */
    LRU_Node LRU_element;        /* Pointer to node in LRU                */
    LFU_Node LFU_element;        /* Pointer to node in LFU                */
} Hash_Node;

typedef struct Hash_Table {      /* The hash table                        */
    int size;                    /* Number of elements in the table       */
    Hash_Node table[MAX_CACHE_ELEMENTS];  /* Array of Hash_Nodes          */
} Hash_Table;

//////////

static LFU_Queue LFU_cache;
static LRU_Queue LRU_cache;
static Hash_Table hash_table;
static int cache_size = 0;

//////////

static void construct_LFU() {
	LFU_cache.size = 0;
	LFU_cache.head = NULL;
	LFU_cache.tail = NULL;
	LFU_cache.min_frequency = 0;
}

//////////

static void construct_LRU() {
	LRU_cache.size = 0;
	LRU_cache.head = NULL;
	LRU_cache.tail = NULL;
}

//////////

static void construct_hash() {
	hash_table.size = 0;
    memset(hash_table.table, 0, MAX_CACHE_ELEMENTS);
}

//////////

void construct_cache() {
	construct_LFU();
	construct_LRU();
	construct_hash();
}

//////////

static int get_LRU_size() {
	return LRU_cache.size;
}

//////////

static int get_LFU_size() {
	return LFU_cache.size;
}

//////////

static int get_hash_size() {
	return hash_table.size;
}

//////////

static int get_cache_bytes_size() {
	return cache_size;
}

//////////

static int get_cache_bytes_available() {
	return (MAX_CACHE_SIZE - cache_size);
}

//////////

static LRU_Node* get_newest_node() {
	return LRU_cache.head;
}

//////////

static LRU_Node* get_oldest_node() {
	return LRU_cache.tail;
}

//////////

static LFU_Node* get_most_freq_node() {
	return LFU_cache.head;
}

//////////

static LFU_Node* get_least_freq_node() {
	return LFU_cache.tail;
}

//////////

/* dbj2 hash function for strings. Adopted from: */
/* http://www.cse.yorku.ca/~oz/hash.html 		 */
static unsigned long hash(char *url) {
	unsigned long hash = 5381;
	int c;

	while ((c = *url++))
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

	return (hash % MAX_CACHE_ELEMENTS);
}

//////////

void addToCache(char *url, char *object, int object_size) {
	printf("add\n");
}

//////////

char* getFromCache(char *url) {
	printf("add\n");
	return 0;
}

//////////
