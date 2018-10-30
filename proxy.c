#include <stdio.h>
#include <string.h>
#include "proxycache.h"
#include "csapp.h"

//////////

#define MAX_OBJECT_SIZE 102400   /* Recommended max object byte size */
#define BODY_SIZE 4096           /* Default response body size       */ 
#define MAX_HEADERS 20           /* Max number of headers supported  */

//////////

void *proxy_communicate(void *fd);
int forward_request(char *req, char *host, char *port);
void forward_cached_response(int clientfd, char *resp, int resp_size);
int forward_response(int clientfd, int serverfd, char **buf, int *buf_size);
int parse_destination(char *dest, char *proto, char *host, char *port, char *path);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
int read_requesthdrs(rio_t *rp, char headers[MAX_HEADERS * sizeof(char *)][MAXLINE * sizeof(char)]);
int build_http_request(char *req, char *method, char *uri, char *host, char *port, char *path, char *proto);

//////////

/* You won't lose style points for including this long line in your code */
static const char *user_agent = "Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3";

int main(int argc, char **argv) {
    int proxy_port;                      /* Port number from command line */
    char *proxy_port_ptr;                /* Port number from command line */
    int listenfd;                        /* File descriptor for listening socket */
    int connfd;                          /* File descriptor for client socket    */
    char port[MAXLINE];                  /* Client port     */
    char hostname[MAXLINE];              /* Client hostname */
    socklen_t clientlen;                 /* Client address info */
    struct sockaddr_storage clientaddr;  /* Client address info */

    memset(port, 0, MAXLINE);
    memset(hostname, 0, MAXLINE);

    /* Ignore SIGPIPE */
    Signal(SIGPIPE, SIG_IGN);

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    /* Validate the port number */
    proxy_port = strtol(argv[1], &proxy_port_ptr, 10);
    if (proxy_port <= 1024 || proxy_port >= 65536) {
        fprintf(stderr, "Proxy port must be greater than 1024 and less than 65536. Port given: %d\n", proxy_port);
        exit(1);
    }

    /* Open and return a listening socket on port */
    listenfd = Open_listenfd(argv[1]);
    if (listenfd == -1 || listenfd == -2){
        perror("Open_listenfd");
    }

    /* Instantiate the cache */
    constructCache();

    while (1) {
        clientlen = sizeof(clientaddr);

        /* Accept a connection on the socket */
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        
        /* Get client name info */
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);

        printf("main: Accepted connection from (%s, %s)\n", hostname, port);
        
        /* Create the thread */
        pthread_t tpid;       
        Pthread_create(&tpid, NULL, proxy_communicate, &connfd);
        Pthread_detach(tpid);

    }

}

//////////

/*
 * Handle one HTTP request/response transaction.
 * This is the main driver of a single lifecycle.
 * Adopted from tiny.c.
 */
void *proxy_communicate(void *clientfd) {
    printf("\n*********ACCEPTING CONNECTION*********\n\n");

    int err = 0;                /* T/F for error catching */
    
    int serverfd;               /* Server socket file descriptor */
    int client_fd;              /* Client socket file descriptor */
    
    rio_t client_rio;           /* Robust wrapper for client IO  */
    
    char req_dest[MAXLINE];     /* Request destination */
    char req_method[MAXLINE];   /* Request method      */
    char req_version[MAXLINE];  /* Request version     */
    char req_header[MAXLINE];   /* First line of incoming request: method, destination, version */
    char req_buf[MAXLINE];      /* Formatted HTTP Request buffer  */
    
    char server_host[MAXLINE];  /* Server hostname          */
    char server_path[MAXLINE];  /* Server path (query)      */
    char server_proto[MAXLINE]; /* Server protocol          */
    char server_port[5];        /* Server port (1024-65536) */
    
    char *resp_buf = NULL;      /* Formatted HTTP Response buffer */
    int resp_buf_size;          /* Size of response buffer        */

    char* cache_object = NULL;  /* Web object returned from cache */
    int cache_object_size;      /* Size of web object returned from cache */
    
    memset(req_dest, 0, MAXLINE);
    memset(req_method, 0, MAXLINE);
    memset(req_version, 0, MAXLINE);
    memset(req_header, 0, MAXLINE);
    memset(server_host, 0, MAXLINE);
    memset(server_path, 0, MAXLINE);
    memset(server_proto, 0, MAXLINE);
    memset(server_port, 0, 5);
    memset(req_buf, 0, MAXLINE);

    client_fd = *(int *)clientfd;

    /* Request headers */
    char req_header_arr[MAX_HEADERS * sizeof(char *)][MAXLINE * sizeof(char)];

    /* Associate client socket connection file descriptor with buffer */
    Rio_readinitb(&client_rio, client_fd);

    /* Read the first line of the request. Return gracefully on error */
    if (!Rio_readlineb(&client_rio, req_header, MAXLINE)) {
        fprintf(stderr, "Proxy could not read first line of the request.\n");
        clienterror(client_fd, "Request headers", "400", "Bad Request", "Proxy cannot process this request");
        return NULL;
    }

    /* Ex: GET, http://localhost:41183/home.html, HTTP/1.1 */
    sscanf(req_header, "%s %s %s", req_method, req_dest, req_version);

    /* Only accept GET requests */
    if (strcasecmp(req_method, "GET")) {
        fprintf(stderr, "Proxy only recognizes GET requests.\n");
        clienterror(client_fd, req_method, "501", "Not Implemented", "Proxy does not implement this method");
        return NULL;
    }

    /* Read headers */
    int num_headers = read_requesthdrs(&client_rio, req_header_arr);
    if (num_headers == -1) {
        clienterror(client_fd, "Request headers", "400", "Bad Request", "Proxy cannot process this request");
        return NULL;
    }

    printf("proxy_communicate: method: %s\n", req_method);
    printf("proxy_communicate: destination: %s\n", req_dest);
    printf("proxy_communicate: version: %s\n", req_version);
    for (int i = 0; i < num_headers; i++){
        printf("proxy_communicate: header: %s", req_header_arr[i]);
    }

    /* Initialize read-write lock */
    pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
    err = pthread_rwlock_init(&rwlock, NULL);
    if (err) {
        clienterror(client_fd, "Lock", "500", "Internal Server Error", "Proxy cannot initialize thread lock");
        return NULL;
    }

    /* Get blocking write lock */
    err = pthread_rwlock_wrlock(&rwlock);
    if (err) {
        clienterror(client_fd, "Lock", "500", "Internal Server Error", "Proxy cannot get a write thread lock");
        return NULL;
    }

    /* Check if object is in the cache */
    getFromCache(req_dest, &cache_object, &cache_object_size);

    /* Unlock blocking read lock */
    err = pthread_rwlock_unlock(&rwlock);
    if (err) {
        clienterror(client_fd, "Lock", "500", "Internal Server Error", "Proxy cannot unlock a write thread lock");
        return NULL;
    }

    /* Forward cached object to the client */
    if (cache_object != NULL) {
        forward_cached_response(client_fd, cache_object, cache_object_size);

        /* Close the socket */
        Close(client_fd);

        return NULL;
    }

    /* Build the HTTP request */
    err = build_http_request(req_buf, req_method, req_dest, server_host, server_port, server_path, server_proto);
    if (err) {
        clienterror(client_fd, "Request", "500", "Internal Server Error", "Proxy cannot process this request");
        return NULL;
    }

    /* Send request */
    serverfd = forward_request(req_buf, server_host, server_port);
    if (serverfd == -1) {
        clienterror(client_fd, "Server connection", "500", "Internal Server Error", "Proxy cannot connect to the server");
        return NULL;
    }

    /* Send the response */
    err = forward_response(client_fd, serverfd, &resp_buf, &resp_buf_size);
    if (err) {
        clienterror(client_fd, "Server connection", "500", "Internal Server Error", "Proxy cannot read server response");
        return NULL;
    }

    /* Store the object in the cache */
    if (resp_buf != NULL) {

        /* Get blocking write lock */
        err = pthread_rwlock_wrlock(&rwlock);
        if (err) {
            clienterror(client_fd, "Lock", "500", "Internal Server Error", "Proxy cannot get a write thread lock");
            return NULL;
        }

        /* Write to cache */
        addToCache(req_dest, resp_buf, resp_buf_size);

        /* Unlock blocking write lock */
        err = pthread_rwlock_unlock(&rwlock);
        if (err) {
            clienterror(client_fd, "Lock", "500", "Internal Server Error", "Proxy cannot unlock a write thread lock");
            return NULL;
        }

    } else {
        clienterror(client_fd, "Server connection", "500", "Internal Server Error", "Proxy cannot read server response");
        return NULL;
    }

    /* Close the socket */
    Close(client_fd);

    return NULL;

}

//////////

/*
 * Make the HTTP request.
 */
int forward_request(char *req, char *host, char *port) {
    printf("\n*********FORWARDING REQUEST*********\n\n");

    int serverfd;   /* Server socket file descriptor */

    /* Open a connection to the server */
    if ((serverfd = open_clientfd(host, port)) == -1){
        perror("open_clientfd");
        return -1;
    }

    /* Write request to the server */
    Rio_writen(serverfd, req, strlen(req));
    
    return serverfd;
}

//////////

/*
 * Send a cached response to the client.
 */
void forward_cached_response(int clientfd, char *resp, int resp_size) {
    Rio_writen(clientfd, resp, resp_size);
}

//////////

/*
 * Send a response to the client.
 */
int forward_response(int clientfd, int serverfd, char **res, int *res_size) {
    printf("\n*********FORWARDING RESPONSE*********\n\n");

    rio_t server_rio;               /* Robust wrapper for server IO                    */
    size_t bytes_read = 0;          /* Number of bytes read from socket response       */
    int add_to_cache  = 1;          /* T/F if object should be added to the cache      */
    size_t total_bytes_read = 0;    /* Total number of bytes read from socket response */


    /* Associate client socket file descriptor with buffer */
    Rio_readinitb(&server_rio, serverfd);

    /* Local buffer for reading each socket response */
    char buf[BODY_SIZE];
    memset(buf, 0, BODY_SIZE);

    /* Local buffer for entire object */
    char resp_buf[MAX_OBJECT_SIZE];
    memset(resp_buf, 0, MAX_OBJECT_SIZE);

    /* Read in response */
    while (1) {
        if ((bytes_read = Rio_readn(serverfd, buf, BODY_SIZE)) == -1) {
            fprintf(stderr, "Error reading server response\n");
            return -1;
        } else {
            if (bytes_read == 0) {
                break;
            } else {
                /* If the object read is too big, do not cache */
                if (total_bytes_read >= MAX_OBJECT_SIZE) {
                    add_to_cache = 0;
                } else {
                    /* Store the object */
                    memcpy((resp_buf + total_bytes_read), buf, bytes_read);
                }
                /* Write the response to the client */
                Rio_writen(clientfd, buf, bytes_read);
                total_bytes_read += bytes_read;
            }
        }
    }

    /* Check if object should be added to the cache */
    if (add_to_cache) {
        /* Create the cache entry */
        *res = (char *)Malloc(total_bytes_read);
        memset(*res, 0, total_bytes_read);
        memcpy(*res, resp_buf, total_bytes_read);
        *res_size = total_bytes_read;
    } 

    return 0;

}

//////////

/*
 * Read HTTP request headers.
 */
int read_requesthdrs(rio_t *rp, char headers[MAX_HEADERS * sizeof(char *)][MAXLINE * sizeof(char)]) {
    int count = 0;

    /* Read first header line */
    Rio_readlineb(rp, headers[count], MAXLINE);

    while(strcmp(headers[count], "\r\n") && count < MAX_HEADERS) {
        count++;
        /* Read each header line */
        Rio_readlineb(rp, headers[count], MAXLINE);
    }
    
    if (count >= MAX_HEADERS) {
        fprintf(stderr, "Number of headers in client request exceeded limit of %d\n", MAX_HEADERS);
        return -1;
    }

    return count;
}

//////////

/*
 *  Generate a new request to send to the server.
 */
int build_http_request(char *req, char *method, char *dest, char *host, char *port, char *path, char *proto) {
    printf("\n*********BUILDING HTTP REQUEST*********\n\n");

    /* T/F for error handling */
    int error = 0;

    error = parse_destination(dest, proto, host, port, path);

    if (error) {
        return error;
    }

    printf("build_http_request: method: %s\n", method);
    printf("build_http_request: dest: %s\n", dest);
    printf("build_http_request: path: %s\n", path);
    printf("build_http_request: protocol: %s\n", proto);
    printf("build_http_request: host: %s\n", host);
    printf("build_http_request: port: %s\n", port);

    /* Format the request */
    sprintf(req, "%s %s %s/1.0\r\n", method, path, proto);
    sprintf(req, "%sHost: %s\r\n", req, host);
    sprintf(req, "%sUser-Agent: %s\r\n", req, user_agent);
    sprintf(req, "%sConnection: close\r\n", req);
    sprintf(req, "%sProxy-Connection: close\r\n\r\n", req);
    
    printf("build_http_request: request is:\n%s", req);

    return error;

}

//////////

/*
 * Given an input destination string, parse out all the relevant 
 * details - the protocol, host, port, and path.
 */
int parse_destination(char *dest, char *proto, char *host, char *port, char *path) {
    printf("\n*********PARSING DESTINATION*********\n\n");
    
    char *get_proto;         /* Get protocol of client request  */
    char *get_port;          /* Get port the client requested   */
    char *get_path;          /* Get the path (query) the client requested */

    size_t diff = 0;

    /* Get the protocol if it exists */
    get_proto = strstr(dest, "://");
    if (get_proto == NULL) {
        /* Default to HTTP */
        strcpy(proto, "http");
    } else {
        /* Get the number of characters for the protocol */
        diff = (get_proto - dest);

        /* Copy protocol */
        memcpy(proto, dest, diff);

        /* Confirm the protocal is HTTP */
        int i = 0;
        while(proto[i]) {
            proto[i] = tolower(proto[i]);
            i++;
        }
        if (strcmp(proto, "http") != 0 ) {
            fprintf(stderr, "Proxy only accepts HTTP requests. Protocol given: %s\n", proto);
            return -1;
        }

        /* Skip "protocol://" */
        dest += (diff + 3);
    }

    /* Get the path */
    get_path = strstr(dest, "/");

    /* Get the number of characters for hostname:port */
    diff = (get_path - dest);

    /* Get the port if it exists */
    get_port = strstr(dest, ":");
    if (get_port == NULL) {
        memcpy(host, dest, diff);
        
        /* Skip hostname */
        dest += diff;

        /* Default to port 80 */
        strcpy(port, "80");
    } else {
        diff = (get_port - dest);
        
        /* Copy the hostname */
        memcpy(host, dest, diff);
        
        /* Skip hostname: */
        dest += (diff + 1);

        /* Skip ":" */
        get_port += 1;
        
        diff = (get_path - get_port);

        /* Copy the port */
        memcpy(port, dest, diff);

        /* Validate the port is an integer */
        int client_port;
        char *client_port_ptr;
        client_port = strtol(port, &client_port_ptr, 10);
        if (client_port == 0) {
            fprintf(stderr, "Server port must be an integer.\n");
            return -1;
        }

        /* Skip port */
        dest += diff;
    }

    /* Copy the path */
    memcpy(path, dest, strlen(dest));

    printf("parse_destination: path: %s\n", path);
    printf("parse_destination: proto: %s\n", proto);
    printf("parse_destination: host: %s\n", host);
    printf("parse_destination: port: %s\n", port);

    return 0;

}

//////////

/*
 * Return an error message to the client.
 * Adopted from tiny.c.
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];
    memset(buf, 0, MAXLINE);
    memset(body, 0, MAXBUF);

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>CS 5450 Proxy</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

//////////
