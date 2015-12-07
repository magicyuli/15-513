#include <sys/mman.h>
#include "csapp.h"

#define DEBUG

#include "contracts.h"

#ifdef DEBUG

#define dbg_printf(...) printf(__VA_ARGS__)

#else

#define dbg_printf(...)

#endif


/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define HOST_MAX_LEN 256
#define PORT_MAX_LEN 5
#define METHOD_MAX_LEN 5
#define VERSION_MAX_LEN 10
#define URI_MAX_LEN 2048

#define NOT_FOUND 404
#define BAD_REQUEST 405
#define SERVER_ERROR 500

#define SPACE " "
#define EMPTY_LINE "\r\n"
#define HD_IGNORE "connection:proxy-connection:user-agent"
#define HD_HOST "host"
#define HTTP_VERSION "HTTP/1.0"

/* Compulsory headers */
static const char *CONST_HEADERS = 
    "Connection: close\r\nProxy-Connection: close\r\n\
User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) \
Gecko/20120305 Firefox/10.0.3\r\n";

/* You won't lose style points for including this long line in your code */
//static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

typedef struct {
    int fd;
    char host[HOST_MAX_LEN];
    char method[METHOD_MAX_LEN];
    char uri[URI_MAX_LEN];
    char headers[MAXLINE];
    char version[VERSION_MAX_LEN];
} req_t;

static void usage() {
    printf("Usage: proxy port\n");
    exit(EXIT_FAILURE);
}

static void lower(char *str) {
    int i;
    for(i = 0; str[i]; i++){
      str[i] = tolower(str[i]);
    }
}

static int parse_req(int connfd, req_t *req) {
    ASSERT(req != NULL);
    
    rio_t rio;
    char buf[MAXLINE];
    char pair[2][MAXLINE];
    char tmpuri[URI_MAX_LEN];
    
    req->fd = connfd;
    
    rio_readinitb(&rio, connfd);
    // parse the first line
    if (rio_readlineb(&rio, buf, MAXLINE) <= 0) {
        return -1;
    }
    sscanf(buf, "%s http://%[^/]/%s %s", req->method, req->host,
                tmpuri, req->version);
    strcpy(req->uri, "/");
    strcat(req->uri, tmpuri);
    if (req->method == NULL || req->uri == NULL || req->version == NULL) {
        return -1;
    }
    
    /** start parse headers **/
    // append the default headers first
    strcat(req->headers, CONST_HEADERS);
    if (rio_readlineb(&rio, buf, MAXLINE) <= 0) {
        return -1;
    }
    // parse request headers
    while (strcmp(buf, EMPTY_LINE)) {
        sscanf(buf, "%[^:]: %s", pair[0], pair[1]);
        lower(pair[0]);
        // host header
        if (!strcmp(HD_HOST, pair[0])) {
            strcpy(req->host, pair[1]);
        }
        // not default header
        else if (!strstr(HD_IGNORE, pair[0])) {
            strcat(req->headers, buf);
        }
        if (rio_readlineb(&rio, buf, MAXLINE) <= 0) {
            return -1;
        }
    }
    
    // append Host header
    char hostheader[HOST_MAX_LEN];
    strcpy(hostheader, "Host: ");
    strcat(hostheader, req->host);
    strcat(req->headers, hostheader);
    strcat(req->headers, EMPTY_LINE);
    strcat(req->headers, EMPTY_LINE);
    /** end parse headers **/
    
    return 0;
}

static int make_request(req_t req) {
    char hostname[HOST_MAX_LEN];
    char port[PORT_MAX_LEN];
    int clientfd;
    char reqstr[MAXLINE];
    int reqlen;
    
    sscanf(req.host, "%[^:]:%[^:]", hostname, port);
    if (strlen(port) == 0) {
        strcpy(port, "80");
    }
    clientfd = Open_clientfd(hostname, port);
    
    sprintf(reqstr, "%s %s %s\r\n", req.method, req.uri, HTTP_VERSION);
    sprintf(reqstr, "%s%s\r\n\r\n", reqstr, req.headers);
    
    reqlen = strlen(reqstr);
    
    if (rio_writen(clientfd, reqstr, reqlen) != reqlen) {
        return -1;
    }
    return clientfd;
}

static void serve(int connfd) {
    req_t req;
    int err = 0;
    int responsefd;
    
    rio_t rio;
    int readlen;
    int reslen = 0;
    char res[MAX_OBJECT_SIZE];
    char buf[MAX_OBJECT_SIZE];
    //char *res;
    
    
    // init struct req
    req.fd = 0;
    strcpy(req.host, "");
    strcpy(req.method, "");
    strcpy(req.uri, "");
    strcpy(req.headers, "");
    strcpy(req.version, "");
    
    if (parse_req(connfd, &req) < 0) {
        err = BAD_REQUEST;
    }
    else {
        dbg_printf("%s %s %s\r\n%s", req.method, req.uri, req.version, req.headers);
        if ((responsefd = make_request(req)) < 0) {
            err = SERVER_ERROR;
        }
        dbg_printf("Started consuming reponse from remote server.\n");
        strcpy(res, "");
        Rio_readinitb(&rio, responsefd);
        while ((readlen = rio_readnb(&rio, buf, MAX_OBJECT_SIZE)) > 0) {
            memcpy(res + reslen, buf, readlen);
            reslen += readlen;
        }
        Close(responsefd);
        // error occurred
        if (readlen < 0) {
            err = SERVER_ERROR;
            perror("Reading response");
        }
        else {
            dbg_printf("Finished consuming reponse from remote server. \
                Following is the response:\n");
            dbg_printf("%s\n", res);
            dbg_printf("Writing reponse to the client. Length: %d\n", reslen);
            if (rio_writen(connfd, res, reslen) != reslen) {
                err = SERVER_ERROR;
                perror("Writing response");
            }
        }
    }
    if (err > 0) {
        // TODO
        printf("error: %d", err);
    }
    Close(connfd);
    
}

//static void new_thread(void *) {
    
//}

static void run_server(char *port) {
    int listenfd;
    int *connfd;
    //char clienthostname[HOST_MAX_LEN], clientport[PORT_MAX_LEN];
    struct sockaddr_storage sockaddr;
    socklen_t socklen;
    
    listenfd = Open_listenfd(port);
    
    while (1) {
        socklen = sizeof(sockaddr);
        connfd = malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *) &sockaddr, &socklen);
        //Getnameinfo((SA *) &sockaddr, socklen, 
            //clienthostname, HOST_MAX_LEN, clientport, PORT_MAX_LEN, 0);
        //printf("Got connection from: %s:%s\n", clienthostname, clientport);
        serve(*connfd);
        free(connfd);
    }
    
    Close(listenfd);
}

int main(int argc, char **argv)
{
    if (argc < 2 || atoi(argv[1]) == 0) {
        usage();
    }
    run_server(argv[1]);
    return 0;
}
