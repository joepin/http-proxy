#include "proxycache.h"

/*
 * README
 * Implementation of a forward cache for a proxy server.
 */

//////////

typedef struct Node {      		 /* Node for table 	 	                 */
    time_t time;                 /* Time node was last accessed          */
    char *buf;         	         /* Buffer for web object                */
    int buf_len;                 /* Size of buffer for web object        */
    char url[MAX_URL_CHARS];     /* URL string 						     */
} Node;

//////////

static Node* cache[MAX_LRU_ELEMENTS];
static int cache_size = 0;
static int num_nodes  = 0;

//////////

/* Helper function to sort cache */
int comp_time(const void * a, const void * b) {
	const Node* node_a = a;
	const Node* node_b = b;
	return difftime(node_a->time, node_b->time);
}

//////////

void constructCache() {
	printf("\n*********CONSTRUCTING CACHE*********\n\n");

	/* Fill the cache with empty nodes */
	for (int i = 0; i < MAX_LRU_ELEMENTS; i++) {
    	printf("Creating node %d for cache\n", i);
    	Node *new_node = Malloc(sizeof(Node));

    	new_node->time = 0;
    	new_node->buf = NULL;
    	new_node->buf_len = 0;
		memset(new_node->url, 0, MAX_URL_CHARS);

		/* Add node to cache */
		cache[i] = new_node;
	}
}

//////////

void addToCache(char *url, char *object, int object_size) {
    printf("\n*********ADDING OBJECT TO CACHE*********\n\n");
    
    printf("Adding object size: %d\n", object_size);
    printf("Current cache size: %d\n", cache_size);
    printf("Available memeory in cache: %d\n", (MAX_CACHE_SIZE - cache_size));
    printf("Is space available: %d\n", (num_nodes != MAX_LRU_ELEMENTS));

    cache[num_nodes]->buf = object;
    cache[num_nodes]->time = time(0);
    strcpy(cache[num_nodes]->url, url);
    cache[num_nodes]->buf_len = object_size;

    num_nodes++;

   	// qsort(cache, MAX_LRU_ELEMENTS, sizeof(struct Node), comp_time);

}

//////////

void getFromCache(char *url, char *object, int *object_size) {
    printf("\n*********GETTING OBJECT FROM CACHE*********\n\n");

    for (int i = 0; i < MAX_LRU_ELEMENTS; i++) {
		printf("time: %lu\n", cache[i]->time);
		printf("len: %d\n", cache[i]->buf_len);
		printf("url: %s\n", cache[i]->url);
    }

}

//////////
