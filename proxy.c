#include <stdio.h>
#include <string.h>
#include "csapp.h"

// @TODO
// - Threads
// - Error handling
// - Cache

#define MAX_HEADERS 20          /* Max number of headers supported */
#define MAX_CACHE_SIZE 1049000  /* Recommended max cache size      */
#define MAX_OBJECT_SIZE 102400  /* Recommended max object size     */

//////////

void proxy_communicate(int fd);
int read_requesthdrs(rio_t *rp, char headers[MAX_HEADERS * sizeof(char *)][MAXLINE * sizeof(char)]);
void build_http_request(char *req, char *method, char *uri, char *host, char *port, char *path, char *proto);
int forward_request(char *req, char *host, char *port, char *buf);
void forward_response(int clientfd, int serverfd, char *buf);
void parse_destination(char *dest, char *proto, char *host, char *port, char *path);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

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

    while (1) {
        clientlen = sizeof(clientaddr);

        /* Accept a connection on the socket */
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        
        /* Get client name info */
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);

        printf("main: Accepted connection from (%s, %s)\n", hostname, port);
        
        /* @TODO: Add threads here */

        /* Handle an HTTP request and response */
        proxy_communicate(connfd);
        
        /* Close the socket */
        Close(connfd);
    }
}

//////////

/*
 * Handle one HTTP request/response transaction.
 * This is the main driver of a single lifecycle.
 * Adopted from tiny.c.
 */
void proxy_communicate(int client_fd) {
    printf("\n*********ACCEPTING CONNECTION*********\n\n");

    int serverfd;               /* Server socket file descriptor */
    rio_t client_rio;           /* Robust wrapper for client IO  */
    char req_dest[MAXLINE];     /* Request destination */
    char req_method[MAXLINE];   /* Request method      */
    char req_version[MAXLINE];  /* Request version     */
    char req_header[MAXLINE];   /* First line of incoming request: method, destination, version */
    char server_host[MAXLINE];  /* Server hostname          */
    char server_path[MAXLINE];  /* Server path (query)      */
    char server_proto[MAXLINE]; /* Server protocol          */
    char server_port[5];        /* Server port (1024-65536) */
    char resp_buf[MAXLINE];     /* Formatted HTTP Request buffer  */
    char req_buf[MAXLINE];      /* Formatted HTTP Response buffer */
    
    /* Request headers */
    char req_header_arr[MAX_HEADERS * sizeof(char *)][MAXLINE * sizeof(char)];

    /* Associate client socket connection file descriptor with buffer */
    Rio_readinitb(&client_rio, client_fd);

    /* Read first line of request. Return gracefully on error */
    if (!Rio_readlineb(&client_rio, req_header, MAXLINE)) {

        /* @TODO: Add clienterror */
        /* @TODO: Kill thread */
        /* @TODO: Close connection */

        return;
    }

    /* Ex: GET, http://localhost:41183/home.html, HTTP/1.1 */
    sscanf(req_header, "%s %s %s", req_method, req_dest, req_version);

    /* Only accept GET requests */
    if (strcasecmp(req_method, "GET")) {
        fprintf(stderr, "proxy_communicate: Proxy only recognizes GET requests.\n");
        clienterror(client_fd, req_method, "501", "Not Implemented", "Proxy does not implement this method");

        /* @TODO: Kill thread */
        /* @TODO: Close connection */

        return;
    }

    /* Read headers */
    int num_headers = read_requesthdrs(&client_rio, req_header_arr);
    if (num_headers == -1) {
        fprintf(stderr, "proxy_communicate: Proxy error reading headers from the client.\n");

        /* @TODO: Add clienterror */
        /* @TODO: Kill thread */
        /* @TODO: Close connection */

        return;
    }

    printf("proxy_communicate: method: %s\n", req_method);
    printf("proxy_communicate: destination: %s\n", req_dest);
    printf("proxy_communicate: version: %s\n", req_version);
    for (int i = 0; i < num_headers; i++){
        printf("proxy_communicate: header: %s", req_header_arr[i]);
    }

    /* Build the HTTP request */
    build_http_request(req_buf, req_method, req_dest, server_host, server_port, server_path, server_proto);

    /* @TODO: Check cache */

    /* Send request */
    serverfd = forward_request(req_buf, server_host, server_port, resp_buf);

    /* @TODO: Add url / objects to cache */
    
    /* Send the response */
    forward_response(client_fd, serverfd, resp_buf);

}

//////////

/*
 * Make the HTTP request.
 */
int forward_request(char *req, char *host, char *port, char *buf) {
    printf("\n*********FORWARDING REQUEST*********\n\n");

    int serverfd;   /* Server socket file descriptor */

    /* Open a connection to the server */
    if ((serverfd = open_clientfd(host, port)) == -1){
        perror("open_clientfd");

        /* @TODO: Add clienterror */
        /* @TODO: Kill thread */
        /* @TODO: Close connection */

    }

    /* Write request to the server */
    Rio_writen(serverfd, req, strlen(req));
    
    return serverfd;
}

//////////

/*
 * Send a response to the client.
 */
void forward_response(int clientfd, int serverfd, char *res) {
    printf("\n*********FORWARDING RESPONSE*********\n\n");

    rio_t server_rio;         /* Robust wrapper for server IO */
    char header_buf[MAXLINE]; /* Buffer for response headers  */
    char resp_len[MAXLINE];   /* Response Content-Length buf  */
    char *get_resp_status;    /* Response Status              */
    char *get_resp_len;       /* Response Content-Length      */
    char *get_resp_end;       /* Response Content-Length end  */
    size_t resp_body_len;     /* Number of bytes in response body */

    /* Associate client socket connection file descriptor with buffer */
    Rio_readinitb(&server_rio, serverfd);

    /* Read "Status" header */
    Rio_readlineb(&server_rio, header_buf, MAXLINE);

    /* Check "Status" */
    get_resp_status = strstr(header_buf, "200");

    /* @TODO - Error checking */
    if (get_resp_status == NULL) {
        printf("forward_response: Server response not equal to 200: %s\n", header_buf);       
    }

    /* Read remaining headers to get "Content-Length" */
    int count = 0;
    while(strcmp(header_buf, "\r\n") && count < MAX_HEADERS) { 
        Rio_readlineb(&server_rio, header_buf, MAXLINE);

        /* Get the the Content-Length if it exists */
        get_resp_len = strstr(header_buf, "Content-Length");

        if (get_resp_len != NULL) {
            /* Get starting position of "Content-Length" value */
            get_resp_len = (strstr(header_buf, ":") + 2);
            get_resp_end = strstr(header_buf, "\r\n");
            size_t diff1 = (get_resp_len - header_buf);
            size_t diff2 = (get_resp_end - get_resp_len);

            /* Get the "Content-Length" value */
            memcpy(resp_len, (header_buf + diff1), diff2);
            break;
        }

        count++;
    }

    /* @TODO - Error handling */
    if (count >= MAX_HEADERS) {
        fprintf(stderr, "forward_response: Proxy received over the max number of headers from the server.\n");
    }

    /* Convert char array to size_t */
    sscanf(resp_len, "%zu", &resp_body_len);

    /* @TODO */
    /* Buffer for the cache. If the size of the buffer exceeds the maximum */
    /* object size, discard the buffer.                                    */ 
    void *cache_buf = Malloc(MAX_OBJECT_SIZE);
    memset(cache_buf, 0, MAX_OBJECT_SIZE);

    /* Free buffer */
    free(cache_buf);
}

//////////

/*
 * Read HTTP request headers.
 */
int read_requesthdrs(rio_t *rp, char headers[MAX_HEADERS * sizeof(char *)][MAXLINE * sizeof(char)]) {
    int count = 0;

    /* Read header line */
    Rio_readlineb(rp, headers[count], MAXLINE);

    while(strcmp(headers[count], "\r\n") && count < MAX_HEADERS) {
        count++;
        /* Read each header line */
        Rio_readlineb(rp, headers[count], MAXLINE);
    }
    
    if (count >= MAX_HEADERS) {

        /* @TODO: Add clienterror */
        /* @TODO: Kill thread */
        /* @TODO: Close connection */

        return -1;
    }

    return count;
}

//////////

/*
 *  Generate a new request to send to the server.
 */
void build_http_request(char *req, char *method, char *dest, char *host, char *port, char *path, char *proto) {

    parse_destination(dest, proto, host, port, path);
    
    printf("\n*********BUILDING HTTP REQUEST*********\n\n");

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

}

//////////

/*
 * Given an input destination string, parse out all the relevant 
 * details - the protocol, host, port, and path.
 */
void parse_destination(char *dest, char *proto, char *host, char *port, char *path) {
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

        /* Validate the port number */
        int client_port;
        char *client_port_ptr;
        client_port = strtol(port, &client_port_ptr, 10);
        if (client_port == 0) {
            fprintf(stderr, "Server port must be an integer.\n");
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

}

//////////

/*
 * Return an error message to the client.
 * Adopted from tiny.c.
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];

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
