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

/* Create NULL Nodes for each cache entry */
void constructCache() {
	printf("\n*********CONSTRUCTING CACHE*********\n\n");

	/* Fill the cache with empty nodes */
	for (int i = 0; i < MAX_LRU_ELEMENTS; i++) {
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

/* Add an object to the cache */
void addToCache(char *url, char *object, int object_size) {
    printf("\n*********ADDING OBJECT TO CACHE*********\n\n");
    
    printf("Adding object size: %d\n", object_size);
    printf("Current cache size: %d\n", cache_size);
    printf("Current number of elements: %d\n", num_nodes);
    printf("Available memeory in cache: %d\n", (MAX_CACHE_SIZE - cache_size));
    printf("Is space available: %d\n", (num_nodes != MAX_LRU_ELEMENTS));

    /* Check if the object fits in the cache */
    if (object_size >= MAX_OBJECT_SIZE) {
    	fprintf(stderr, "Could not add cache entry due to the object size\n");
    	return;
    }

    /* Check if cache is empty */
    if (num_nodes == 0) {
	    cache[num_nodes]->buf = object;
	    cache[num_nodes]->time = time(NULL);
	    strcpy(cache[num_nodes]->url, url);
	    cache[num_nodes]->buf_len = object_size;

	    /* Update counts */
	    num_nodes++;
	    cache_size += object_size;

		return;
    }

    /* Check if there is room in the cache for the object */
    int bytes_available = (MAX_CACHE_SIZE - cache_size);

    /* Get oldest node index */
	int oldest_node_index = num_nodes - 1;

    /* Check if the object fits in the cache / evict nodes to make space */
	while ( (bytes_available < object_size) && oldest_node_index >= 0) {
		/* Evict each node */
		Node *evicted_node = cache[oldest_node_index];

		/* Free the stored buffer */
		free(evicted_node->buf);

		/* Updated the cache sizes */
		cache_size -= evicted_node->buf_len;
		num_nodes--;

		/* Set node to default values */
		evicted_node->buf = NULL;
		evicted_node->buf_len = 0;
		evicted_node->time = 0;
		memset(evicted_node->url, 0, MAX_URL_CHARS);

		/* Check next oldest node */
		oldest_node_index--;
	}

    /* Check if the object fits in the cache after evictions */
    if (bytes_available < object_size) {
    	fprintf(stderr, "Could not add cache entry due to lack of memory\n");
    	return;
    }

    /* Check if the cache is full */
    if (num_nodes == MAX_LRU_ELEMENTS) {
		/* Evict the oldest node node */
		Node *evicted_node = cache[oldest_node_index];

		/* Free the stored buffer */
		free(evicted_node->buf);

		/* Updated the cache sizes */
		cache_size -= evicted_node->buf_len;
		num_nodes--;

		/* Set node to default values */
		evicted_node->buf = NULL;
		evicted_node->buf_len = 0;
		evicted_node->time = 0;
		memset(evicted_node->url, 0, MAX_URL_CHARS);
    } 

    /* Set the new node values */
    cache[oldest_node_index]->buf = object;
    cache[oldest_node_index]->time = time(NULL);
    strcpy(cache[oldest_node_index]->url, url);
    cache[oldest_node_index]->buf_len = object_size;

    /* Update counts */
    num_nodes++;
    cache_size += object_size;

    /* Sort the cache */
   	qsort(cache, MAX_LRU_ELEMENTS, sizeof(Node*), comp_time);

}

//////////

/* Get an object from the cache */
void getFromCache(char *url, char *object, int *object_size) {
    printf("\n*********GETTING OBJECT FROM CACHE*********\n\n");

    /* Value returned from the table */
	Node *node = NULL;

	/* Search table for url */
    for (int i = 0; i < MAX_LRU_ELEMENTS; i++) {
    	if (strcmp(cache[i]->url, url) != 0) {
			node = cache[i];
			break;
    	}
    }

	/* Object is not in the cache */ 
	if (node == NULL) {
		printf("Node not found in cache.\n");
		return;
	}

	/* Set object */
	object = node->buf;

	/* Update the time */
	node->time = time(NULL);

	/* Reorder the cache */
   	qsort(cache, MAX_LRU_ELEMENTS, sizeof(Node*), comp_time);

}

//////////
