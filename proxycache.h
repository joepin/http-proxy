#ifndef _PROXY_CACHE_H_
#define _PROXY_CACHE_H_

#include <time.h>
#include <stdio.h>

#define MAX_CACHE_SIZE 1049000   /* Recommended max cache byte size      */
#define MAX_CACHE_ELEMENTS 1000  /* Supported number of cache elements   */
#define MAX_LFU_ELEMENTS 3       /* Supported number of LFU elements     */
#define MAX_LRU_ELEMENTS 997     /* Supported number of LRU elements     */
#define MAX_URL_CHARS 2000       /* Supported chars for URL's            */

//////////

/* Constructors */
void construct_cache();

/* Main cache functions */
void addToCache(char *url, char *object, int object_size);
char* getFromCache(char *url);

//////////

#endif