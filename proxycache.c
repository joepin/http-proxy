#include "proxycache.h"

/*
 * README
 * Implementation of a forward cache for a proxy server. Creates
 * one table for LRU and LFU functionality.
 */

//////////

typedef struct Node {      		 /* Node for table 	 	                 */
    int frequency;               /* Frequency count of URL               */
    time_t time;                 /* Time node was last accessed          */
    char *buffer;                /* Buffer for web object                */
    int buf_len;                 /* Size of buffer for web object        */
    char url[MAX_URL_CHARS];     /* URL string 						     */
} Node;

typedef struct Table {      	 /* Table       	                      */
    int size;                    /* Number of elements in the table       */
	int capacity;				 /* Total capacity of cache 			  */
	int cache_size;				 /* Size of the cache in bytes            */
	int LRU_size;			 	 /* Number of elements in LRU 	  	      */
	int LRU_capacity;			 /* Capacity of LRU 					  */
    Node *LRU_cache[MAX_LRU_ELEMENTS]; 		  	/* LRU cache 			  */
} Table;

//////////

static Table table;    			 /* Table 					 			  */

//////////

void constructCache() {
	table.capacity = MAX_CACHE_ELEMENTS;
	table.LRU_capacity = MAX_LRU_ELEMENTS;
}

//////////

/* Helper function to sort LRU cache */
int cmpfuncLRU(const void * a, const void * b) {
   return ( *(int*)a.time - *(int*)b.time );
}

//////////

/* Add an object to the table, in either the LRU or LFU caches.   	   */
/* There should not be redundant entries between each cache, therefore */
/* each object is only added to one cache. 							   */
void addToCache(char *url, char *object, int object_size) {
    printf("\n*********ADDING OBJECT TO CACHE*********\n\n");

    /* Check if the object fits in the cache */
    if (object_size >= MAX_OBJECT_SIZE) {
    	fprintf(stderr, "Could not add cache entry due to the object size\n");
    	return;
    }

    /* Check if there is room in the cache */
    int bytesAvailable = (MAX_CACHE_SIZE - table.cache_size);

    /* Check if the object fits in the cache and evict nodes to make space */
	int evicted_node_index = table.LRU_size - 1;
	while ( (bytesAvailable < object_size) || evicted_node_index >= 0) {
		Node *evicted_node = table.LRU_cache[evicted_node_index];
		table.cache_size -= evicted_node->buf_len;
		table.LRU_size--;
		table.size--;
		free(evicted_node->buffer);
		free(evicted_node);
		bytesAvailable = (MAX_CACHE_SIZE - table.cache_size);
	}

    /* Check if the object fits in the cache after evictions */
    if (bytesAvailable < object_size) {
    	fprintf(stderr, "Could not add cache entry due to lack of memory\n");
    	return;
    }

    /* Instantiate a hash table entry */
    Node *node = Malloc(sizeof(struct Node));
    node->frequency = 1;
    node->buffer = object; 
    strcpy(node->url, url);
    node->time = time(NULL);
    node->buf_len = object_size;

    /* Get oldest node index */
	int oldest_node_index = table.LRU_size - 1;

    /* Check if the cache is full */
    if (table.LRU_size == MAX_LRU_ELEMENTS) {
    	/* Evict the oldest node */
		Node *evicted_node = table.LRU_cache[oldest_node_index];
		table.cache_size -= evicted_node->buf_len;
		table.LRU_size--;
		table.size--;
		free(evicted_node->buffer);
		free(evicted_node);
    } 

	/* Set the new node */
	table.LRU_cache[oldest_node_index] = node;

	/* Update the size counts */
	table.cache_size += object_size;
	table.size++;
	table.LRU_size++;

	/* Reorder the LRU cache */
	qsort(table.LRU_cache, table.LRU_size, sizeof(struct Node), cmpfuncLRU);

}

//////////

/* Return an object from the cache. Update the object's frequency    */
/* and access time within the cache.  								 */
void getFromCache(char *url, char *object, int *object_size) {
    printf("\n*********GETTING OBJECT FROM CACHE*********\n\n");

	Node *node;		    /* Value returned from table 	  */

	/* Check if object is in the LRU cache */
	for (int i = 0; i < table.LRU_size; i++) {
		/* Search for the stored URL */
		if (strcmp(table.LRU_cache[i]->url, url)) {
			node = table.LRU_cache[i];
		}
	}

	/* Object is not in the cache */
	if (node == NULL) {
		return;
	}

	/* Set object */
	object = node->buffer;

	/* Update the frequency and time */
	node->frequency++;
	node->time = time(NULL);

	/* Reorder the LRU cache */
	qsort(table.LRU_cache, table.LRU_size, sizeof(struct Node), cmpfuncLRU);

}

//////////
