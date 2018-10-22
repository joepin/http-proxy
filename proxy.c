#include <stdio.h>
#include <string.h>
#include "csapp.h"

void doit(int);
int read_requesthdrs(rio_t *rp, char *heads[], int size);
char* build_http_request(char *method, char *uri, char **heads);
int parse_destination(char *dest, char *proto, char *host, char *port, char *uri);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum,
        char *shortmsg, char *longmsg);

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_HEADERS 20

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

int main(int argc, char **argv)
{
  printf("%s", user_agent_hdr);

  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

    /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
//  int counter = 1;
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
//    char message[] = "hello";
//    char m2[100];
//    strcpy(m2, message);
//    sprintf(m2, strcat(m2, " %lu"), sizeof(message));
//    fprintf(stdout, "%s\n", m2);
//    char buf[10000] = "HTTP/1.0 200 OK \r\nDate: Tue, 16 Oct 2018 9:27:32 GMT \r\nContent-Length: 8 \r\nContent-Type: text/html \r\nConnection: Closed \r\n\nhello\n\r\n";
//    fprintf(stdout, "%d\n", counter);
    doit(connfd);
//    Write(connfd, buf, sizeof(buf));
    Close(connfd);
  }
}

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd)
{
    char buf[MAXLINE], method[MAXLINE], destination[MAXLINE], version[MAXLINE];
    rio_t rio;
    char **heads = malloc(MAX_HEADERS * sizeof(char *));

    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))
        return;
    printf("HERE:\n%s", buf);
    sscanf(buf, "%s %s %s", method, destination, version);
    printf("after scanf:\n%s, %s, %s:\n", method, destination, version);
    printf("compared: %d\n", strcasecmp(method, "GET"));
    if (strcasecmp(method, "GET") && strcasecmp(method, "CONNECT")) {
        printf("this is an error\n");
        clienterror(fd, method, "501", "Not Implemented",
                    "Proxy does not implement this method");
        // TODO: implement closing connection here
        return;
    }
    int num_headers = read_requesthdrs(&rio, heads, MAX_HEADERS);
    printf("num headers: %d\n", num_headers);

    for (int i = 0; i < num_headers; i++){
        printf("here %d; %s\n", i, heads[i]);
    }

    // TODO: parse uri path to cache objects with

    char *http_request = build_http_request(method, destination, heads);

}
/* $end doit */

/*
 * read_requesthdrs - read HTTP request headers
 */
/* $begin read_requesthdrs */
int read_requesthdrs(rio_t *rp, char **hs, int size)
{
    int count = 0;
    hs[count] = malloc(MAXLINE * sizeof(char));

    Rio_readlineb(rp, hs[count], MAXLINE);
    printf("in read_request headers %s", hs[count]);
    while(strcmp(hs[count], "\r\n") && count < size) {
        printf("count is: %d, hs has: %s\n", count, hs[count]);
        count++;
        hs[count] = malloc(MAXLINE * sizeof(char));
        Rio_readlineb(rp, hs[count], MAXLINE);
        printf("%s", hs[count]);
    }
    if (count >= size) {
        return -1;
    }

    return count;
}
/* $end read_requesthdrs */

/*
 * build_http_request - generate a new request to send to server
 * */

char* build_http_request(char *method, char *dest, char **heads) {
    char *protocol = malloc(10);
    char *host = malloc(MAXLINE);
    char *port = malloc(10);
    char *uri = malloc(MAXLINE);

    char *req = malloc(MAXLINE);

    int have_host = parse_destination(dest, protocol, host, port, uri);
    if (have_host > 0) {
//        printf("%s\n", protocol);
//        printf("%s\n", host);
//        printf("%s\n", port);
//        printf("%s\n", uri);
        if (strlen(port) == 0) {
            sprintf(req, "%s %s://%s%s HTTP/1.0\r\n", method, protocol, host, uri);
            printf("if req is:\n%s\n", req);
        } else {
            sprintf(req, "%s %s://%s:%s%s HTTP/1.0\r\n", method, protocol, host, port, uri);
            printf("else req is:\n%s\n", req);
        }
    } else {
        // TODO: get host from headers
        printf("%s\n", uri);
    }

    return req;
}

/*
 *  parse_destination - given an input destination string, parse out
 *  all the relevant details - the protocol, host, port, and uri
 *  returns 0 if dest is just the uri; 1 otherwise
 * */
int parse_destination(char *dest, char *proto, char *host, char *port, char *uri) {
    char *no_proto = malloc(MAXLINE);
    char *no_host = malloc(MAXLINE);
    char *unclean_host = malloc(MAXLINE);
    char *maybe_port = malloc(6);
    int diff = 0;

    // get the string beginning with the matched substring
    no_proto = strstr(dest, "://");
    if (no_proto == NULL) {
        memcpy(uri, dest, strlen(dest));
        return 0;
    }
    // get the amount of characters from beginning of dest through no_proto
    diff = (int) (no_proto - dest);
    // printf("%d\n", diff);
    // copy diff number of characters from dest into proto
    memcpy(proto, dest, diff);

    // move forward three characters in the no_proto -
    // this is needed in order to get rid of "://"
    no_proto += 3;
    // no_host holds everything past the host & port == just the uri
    no_host = strstr(no_proto, "/");
    // printf("%d; %s\n", diff2, no_host);
    // get the amount of chars from beginning of no_proto through no_host
    diff = (int) (no_host - no_proto);

    memcpy(unclean_host, no_proto, diff);

    maybe_port = strstr(unclean_host, ":");
    // printf("maybe: %s\n", maybe_port);
    if (maybe_port == NULL) {
        // printf("here\n");
        memcpy(host, unclean_host, strlen(unclean_host));
        memset(port, 0, sizeof(*port));
    } else {
        diff = (int) (maybe_port - unclean_host);
        memcpy(host, unclean_host, diff);
        maybe_port += 1;
        memcpy(port, maybe_port, strlen(maybe_port));

    }

    memcpy(uri, no_host, strlen(no_host));
    return 1;
}

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {  /* Static content */ //line:netp:parseuri:isstatic
        strcpy(cgiargs, "");                             //line:netp:parseuri:clearcgi
        strcpy(filename, ".");                           //line:netp:parseuri:beginconvert1
        strcat(filename, uri);                           //line:netp:parseuri:endconvert1
        if (uri[strlen(uri)-1] == '/')                   //line:netp:parseuri:slashcheck
            strcat(filename, "home.html");               //line:netp:parseuri:appenddefault
        return 1;
    }
    else {  /* Dynamic content */                        //line:netp:parseuri:isdynamic
        ptr = index(uri, '?');                           //line:netp:parseuri:beginextract
        if (ptr) {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        }
        else
            strcpy(cgiargs, "");                         //line:netp:parseuri:endextract
        strcpy(filename, ".");                           //line:netp:parseuri:beginconvert2
        strcat(filename, uri);                           //line:netp:parseuri:endconvert2
        return 0;
    }
}
/* $end parse_uri */

/*
 * serve_static - copy a file back to the client
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    get_filetype(filename, filetype);       //line:netp:servestatic:getfiletype
    sprintf(buf, "HTTP/1.0 200 OK\r\n");    //line:netp:servestatic:beginserve
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));       //line:netp:servestatic:endserve
    printf("Response headers:\n");
    printf("%s", buf);

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0);    //line:netp:servestatic:open
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);//line:netp:servestatic:mmap
    Close(srcfd);                           //line:netp:servestatic:close
    Rio_writen(fd, srcp, filesize);         //line:netp:servestatic:write
    Munmap(srcp, filesize);                 //line:netp:servestatic:munmap
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}
/* $end serve_static */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    if (Fork() == 0) { /* Child */ //line:netp:servedynamic:fork
        /* Real server would set all CGI vars here */
        setenv("QUERY_STRING", cgiargs, 1); //line:netp:servedynamic:setenv
        Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ //line:netp:servedynamic:dup2
        Execve(filename, emptylist, environ); /* Run CGI program */ //line:netp:servedynamic:execve
    }
    Wait(NULL); /* Parent waits for and reaps child */ //line:netp:servedynamic:wait
}
/* $end serve_dynamic */

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */
