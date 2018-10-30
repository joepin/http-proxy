#ifndef _PROXY_CACHE_H_
#define _PROXY_CACHE_H_

#include <time.h>
#include <stdio.h>
#include "csapp.h"
#include <string.h>

#define MAX_CACHE_SIZE 1049000   /* Recommended max cache size           */
#define MAX_CACHE_ELEMENTS 1000  /* Supported number of cache elements   */
#define MAX_LFU_ELEMENTS 3       /* Supported number of LFU elements     */
#define MAX_LRU_ELEMENTS 997     /* Supported number of LRU elements     */
#define MAX_URL_CHARS 2000       /* Supported chars for URL's            */
#define MAX_OBJECT_SIZE 102400   /* Recommended max object byte size     */

//////////

/* Main cache functions */
void constructCache();
void addToCache(char *url, char *object, int object_size);
void getFromCache(char *url, char **object, int *object_size);

//////////

#endif
