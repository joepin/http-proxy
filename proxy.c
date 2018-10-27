#include <stdio.h>
#include <string.h>
#include "csapp.h"

void doit(int);
int read_requesthdrs(rio_t *rp, char *heads[], int size);
char* build_http_request(char *method, char *uri, char **heads, char *host, char *port);
int make_request(char *req, char *host, char *port, char *buf);
void send_response(int fd, char*buf);
int parse_destination(char *dest, char *proto, char *host, char *port, char *uri);
int parse_uri(char *uri, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
/* Max number of headers supported */
#define MAX_HEADERS 20

/* You won't lose style points for including this long line in your code */
static const char *user_agent = "Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3";

int main(int argc, char **argv)
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
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
        doit(connfd);
        
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
void doit(int fd)
{
    printf("\n*********ACCEPTING CONNECTION*********\n\n");

    char buf[MAXLINE], method[MAXLINE], destination[MAXLINE], version[MAXLINE];
    rio_t rio;

    /* Malloc space to hold headers */
    char **heads = Malloc(MAX_HEADERS * sizeof(char *));

    /* Associate socket connection file descriptor with buffer */
    Rio_readinitb(&rio, fd);

    /* Read request. Return gracefully on error */
    if (!Rio_readlineb(&rio, buf, MAXLINE))
        return;

    /* Ex: GET, http://localhost:41183/home.html, HTTP/1.1 */
    sscanf(buf, "%s %s %s", method, destination, version);

    /* Only accept GET requests */
    if (strcasecmp(method, "GET")) {
        fprintf(stderr, "doit: Proxy only recognizes GET requests.\n");
        clienterror(fd, method, "501", "Not Implemented", "Proxy does not implement this method");

        /* @TODO: Kill thread */
        /* @TODO: Close connection */

        return;
    }

    /* Read headers */
    int num_headers = read_requesthdrs(&rio, heads, MAX_HEADERS);
    if (num_headers == -1) {
        fprintf(stderr, "doit: Proxy received over the max number of headers from the client.\n");

        /* @TODO: Kill thread */
        /* @TODO: Close connection */

        return;
    }

    printf("doit: method: %s\n", method);
    printf("doit: destination: %s\n", destination);
    printf("doit: version: %s\n", version);
    for (int i = 0; i < num_headers; i++){
        printf("doit: header: %s", heads[i]);
    }

    /* @TODO: Add URI to cache */

    /* @TODO: Need Malloc? */
    char *host = Malloc(MAXLINE);
    /* Accept ports between 1,024 and 65,536 */
    char *port = Malloc(3);
    char req_buf[MAXLINE];

    /* Build the HTTP request */
    char *http_request = build_http_request(method, destination, heads, host, port);
    
    /* Make the request */
    make_request(http_request, host, port, req_buf);
    
    /* Send the request */
    send_response(fd, req_buf);
    
}

//////////

/*
 * Make the HTTP request.
 */
int make_request(char *req, char *host, char *port, char *buf) {
    int serverfd;
    rio_t rio;

    printf("\n*********MAKING REQUEST*********\n\n");
    
    printf("make_request: host: %s, port: %s\n", host, port);
    printf("make_request: req: %s\n", req);

    /* Open a connection to the server */
    if ((serverfd = open_clientfd(host, port)) == -1){
        perror("open_clientfd");
    }

    /* Write request to the server */
    Rio_writen(serverfd, req, strlen(req));
    
    printf("make_request: wrote request to server\n");

    /* Read server response */
    int readAllData = 0;
    Rio_readinitb(&rio, serverfd);
    while(!readAllData) {
        if (!Rio_readlineb(&rio, buf, MAXLINE)) {
            printf("here error returning");
            return -1;
        }
        printf("Hello bellow:\n%s", buf);
        if (strcmp(buf, "\r\n\r\n")) {
            readAllData = 1;
        }
    }
    return serverfd;
}

//////////

/*
 * Send a response to the client.
 */
void send_response(int clientfd, char *res) {
    printf("\n*********SENDING RESPONSE*********\n\n");
    printf("send_response: res: %s\n", res);
    Rio_writen(clientfd, res, strlen(res));
    printf("send_response: wrote response\n");
}

//////////

/*
 * Read HTTP request headers.
 */
int read_requesthdrs(rio_t *rp, char **hs, int size)
{
    int count = 0;

    /* Malloc and read a header line */
    hs[count] = Malloc(MAXLINE * sizeof(char));
    Rio_readlineb(rp, hs[count], MAXLINE);

    while(strcmp(hs[count], "\r\n") && count < size) {
        count++;
        /* Malloc and read each header line */
        hs[count] = Malloc(MAXLINE * sizeof(char));
        Rio_readlineb(rp, hs[count], MAXLINE);
    }
    
    if (count >= size) {
        return -1;
    }

    return count;
}

//////////

/*
 *  Generate a new request to send to the server.
 */
char* build_http_request(char *method, char *dest, char **heads, char *host, char *port) {
    /* @TODO - Need Malloc? */
    char *protocol = Malloc(10);
    char *uri = Malloc(MAXLINE);
    char *req = Malloc(MAXLINE);

    int have_host = parse_destination(dest, protocol, host, port, uri);
    
    printf("\n*********BUILDING HTTP REQUEST*********\n\n");

    /* Destination did not have a protocol - assume hostname was given */
    if (have_host > 0) {
        printf("build_http_request: destination did not have a protocol\n");
    } else {
        char *host_header = heads[0] + 6;
        printf("%s\n", host_header);
        printf("%s\n", uri);
        char *maybe_port = malloc(6);
        maybe_port = strstr(host_header, ":");

        if (maybe_port == NULL) {
            memcpy(host, host_header, strlen(host_header));
            /* If port is empty, set it to default of 80 */
            strcpy(port, "80");
        } else {
            printf("%s\n", maybe_port);
            int diff = (int) (maybe_port - host_header);
            memcpy(host, host_header, diff);
            maybe_port += 1;
            memcpy(port, maybe_port, strlen(maybe_port));
        }

        printf("Host: %s; Port: %s\n", host, port);

        /* Default to HTTP protocol if none is given */
        memcpy(protocol, "http", strlen("http"));
    }

    printf("build_http_request: method: %s\n", method);
    printf("build_http_request: dest: %s\n", dest);
    printf("build_http_request: protocol: %s\n", protocol);
    printf("build_http_request: host: %s\n", host);
    printf("build_http_request: port: %s\n", port);
    printf("build_http_request: uri: %s\n", uri);

    /* Format the request */
    sprintf(req, "%s %s HTTP/1.0\r\n", method, uri);
    sprintf(req, "%sHost: %s\r\n", req, host);
    sprintf(req, "%sUser-Agent: %s\r\n", req, user_agent);
    sprintf(req, "%sConnection: close\r\n", req);
    sprintf(req, "%sProxy-Connection: close\r\n\r\n", req);
    
    printf("build_http_request: request is:\n%s", req);

    /* @TODO - Free */

    return req;
}

//////////

/*
 * Given an input destination string, parse out all the relevant 
 * details - the protocol, host, port, and uri.
 * Returns 0 if dest is just the uri; 1 otherwise.
 */
int parse_destination(char *dest, char *proto, char *host, char *port, char *uri) {
    printf("\n*********PARSING DESTINATION*********\n\n");

    /* @TODO - Need Malloc? */
    char *no_proto;
    char *no_host = Malloc(MAXLINE);
    char *unclean_host = Malloc(MAXLINE);
    char *maybe_port = Malloc(3);
    int diff = 0;

    /* Get the string beginning with the matched substring */
    no_proto = strstr(dest, "://");
    if (no_proto == NULL) {
        memcpy(uri, dest, strlen(dest));
        return 0;
    }
    
    /* Get the amount of characters from beginning of dest through no_proto */
    diff = (int) (no_proto - dest);

    /* Copy diff number of characters from dest into proto */
    memcpy(proto, dest, diff);

    /* Move forward three characters in the no_proto -  */
    /* this is needed in order to get rid of "://"      */
    no_proto += 3;
    
    /* no_host holds everything past the host & port == just the uri */
    no_host = strstr(no_proto, "/");
    
    /* Get the amount of chars from beginning of no_proto through no_host */
    diff = (int) (no_host - no_proto);

    memcpy(unclean_host, no_proto, diff);

    maybe_port = strstr(unclean_host, ":");

    if (maybe_port == NULL) {
        memcpy(host, unclean_host, strlen(unclean_host));
        /* Default to port 80 */
        strcpy(port, "80");
    } else {
        diff = (int) (maybe_port - unclean_host);
        memcpy(host, unclean_host, diff);
        maybe_port += 1;
        memcpy(port, maybe_port, strlen(maybe_port));
    }

    printf("parse_destination: dest: %s\n", dest);
    printf("parse_destination: proto: %s\n", proto);
    printf("parse_destination: host: %s\n", host);
    printf("parse_destination: port: %s\n", port);
    printf("parse_destination: uri: %s\n", uri);

    memcpy(uri, no_host, strlen(no_host));

    /* @TODO - Free */

    return 1;
}

//////////

/*
 * Return an error message to the client.
 * Adopted from tiny.c.
 */
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
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
