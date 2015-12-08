/**
 * Can handle real pages :D
 * 
 * This file implements a multi-threaded, cached backed
 * proxy server, which gets requests, requests real servers,
 * and forwards back the responses.
 * 
 * The operation routine for this server is as follows:
 *      1. Configure in main;
 *      2. Run server, listen on a port;
 *      3. Accept a client connection;
 *      4. Spin up a thread to serve the client;
 *      5. The thread parses request, gets response from the cache
 *          or makes request to the real server, then forwards back
 *          the results.
 *      6. Repeat 3 - 5 till the server gets shut down.
 * 
 * 
 * Liruoyang YU
 * liruoyay
 */

#include "csapp.h"
#include "cache.h"
#include "contracts.h"
#include "debug.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define HOST_MAX_LEN 256
#define PORT_MAX_LEN 5
#define METHOD_MAX_LEN 5
#define VERSION_MAX_LEN 10
#define URI_MAX_LEN 2048

#define BAD_REQUEST "405 BAD REQUEST"
#define SERVER_ERROR "500 SERVER ERROR"

#define EMPTY_LINE "\r\n"
#define HD_IGNORE "connection:proxy-connection:user-agent"
#define HD_HOST "host"
#define HTTP_VERSION "HTTP/1.0"

/* 
 * Request type. 
 * Instances represent web requests. 
 */
typedef struct {
    int fd;
    char host[HOST_MAX_LEN];
    char method[METHOD_MAX_LEN];
    char uri[URI_MAX_LEN];
    char headers[MAXLINE];
    char version[VERSION_MAX_LEN];
} req_t;

/*************************
 * Start global variables
 *************************/
/* Compulsory headers */
static const char *CONST_HEADERS = 
    "Connection: close\r\nProxy-Connection: close\r\n\
User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) \
Gecko/20120305 Firefox/10.0.3\r\n";
/* Pointer to the cache instance */
static cache_t *csh;
/* The listen fd. Made global for cleaning up */
static int listenfd;
/*************************
 * End global variables
 *************************/
 
static void cleanup(void);

/*************************
 * Start signal handlers
 *************************/
/*
 * SIGPIPE handler.
 * Ignore sigpipe.
 */
static void sigpipe_handler(int sig) {
#ifdef DEBUG
    sio_puts("Received sigpipe\n");
#endif
    /* do nothing */
}

/*
 * SIGINT handler.
 * Clean up then exit.
 */
static void sigint_handler(int sig) {
#ifdef DEBUG
    sio_puts("Received sigint\n");
#endif
    cleanup();
    exit(0);
}

/*
 * SIGTERM handler.
 * Clean up then exit.
 */
static void sigterm_handler(int sig) {
#ifdef DEBUG
    sio_puts("Received sigterm\n");
#endif
    cleanup();
    exit(0);
}
/*************************
 * End signal handlers
 *************************/

/*
 * Print usage info.
 */
static void usage() {
    printf("Usage: proxy <port>\n");
    exit(EXIT_FAILURE);
}

/*
 * Convert string to lower case.
 */
static void lower(char *str) {
    int i;
    for(i = 0; str[i]; i++){
      str[i] = tolower(str[i]);
    }
}

/*
 * Clean up by closing the listen fd
 * and freeing the cache.
 */
static void cleanup(void) {
    if (close(listenfd) < 0) {
        perror("Cleanup - close listenfd");
    }
    free_cache(csh);
}

/*
 * Respond with error pages when errors happen.
 */
static void resp_error(char *errstatus, int fd) {
    char buf[MAXLINE];
    char body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><head><title>Error</title></head>");
    sprintf(body, "%s<body>\r\n", body);
    sprintf(body, "%s%s\r\n", body, errstatus);
    sprintf(body, "%s<p>%s</p></body></html>\r\n", body, strerror(errno));

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s\r\n", errstatus);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    rio_writen(fd, buf, strlen(buf));
    rio_writen(fd, body, strlen(body));
    
    if (close(fd) < 0) {
        perror("resp error - close connfd");
    }
}

/*
 * Parse the incoming request into a request 
 * struct instance.
 */
static int parse_req(int connfd, req_t *req) {
    ASSERT(req != NULL);
    
    rio_t rio;
    char buf[MAXLINE];
    char pair[2][MAXLINE];
    char tmpuri[URI_MAX_LEN];
    
    req->fd = connfd;
    
    rio_readinitb(&rio, connfd);
    
    /* parse the first line */
    if (rio_readlineb(&rio, buf, MAXLINE) <= 0) {
        return -1;
    }
    sscanf(buf, "%s http://%[^/ ]/%s %s", req->method, req->host,
                tmpuri, req->version);
                
    /* deal with the http://hostname:port/ case */
    if (strlen(req->version) == 0) {
        strcpy(req->version, tmpuri);
        strcpy(tmpuri, "");
    }
    
    /* append '/' before the uri */
    strcpy(req->uri, "/");
    strcat(req->uri, tmpuri);
    
    /* malformatted request */
    if (req->method == NULL || req->uri == NULL || req->version == NULL) {
        return -1;
    }
    
    /********************** 
     * start parse headers 
     **********************/
    /* append the default headers first */
    strcat(req->headers, CONST_HEADERS);
    if (rio_readlineb(&rio, buf, MAXLINE) <= 0) {
        return -1;
    }
    
    /* parse request headers */
    while (strcmp(buf, EMPTY_LINE)) {
        sscanf(buf, "%[^:]: %s", pair[0], pair[1]);
        lower(pair[0]);
        
        /* host header */
        if (!strcmp(HD_HOST, pair[0])) {
            strcpy(req->host, pair[1]);
        }
        /* not default header */
        else if (!strstr(HD_IGNORE, pair[0])) {
            strcat(req->headers, buf);
        }
        if (rio_readlineb(&rio, buf, MAXLINE) <= 0) {
            return -1;
        }
    }
    
    /* append Host header */
    char hostheader[HOST_MAX_LEN];
    strcpy(hostheader, "Host: ");
    strcat(hostheader, req->host);
    strcat(req->headers, hostheader);
    strcat(req->headers, EMPTY_LINE);
    strcat(req->headers, EMPTY_LINE);
    /********************** 
     * End parse headers 
     **********************/
    
    return 0;
}

/*
 * Make a request to the host with the headers
 * in the given the request instance.
 */
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
    if ((clientfd = open_clientfd(hostname, port)) < 0) {
        return -1;
    }
    
    sprintf(reqstr, "%s %s %s\r\n", req.method, req.uri, HTTP_VERSION);
    sprintf(reqstr, "%s%s\r\n\r\n", reqstr, req.headers);
    
    reqlen = strlen(reqstr);
    
    if (rio_writen(clientfd, reqstr, reqlen) != reqlen) {
        return -1;
    }
    return clientfd;
}

/*
 * Core function for serving the client.
 * This is done by
 *      1. parsing the client's request;
 *      2. making request to the real server or getting from cache;
 *      3. forwarding responses to the client.
 */
static void serve(int connfd) {
    req_t req;                  /* request instance */
    char *err = NULL;           /* error status */
    int responsefd;             /* fd for the real server */
    rio_t rio;
    int readlen;                /* number of bytes read into buffer */ 
    char buf[MAX_OBJECT_SIZE];  /* buffer */
    int reslen = 0;             /* response size */
    char *res;                  /* potential cache */
    
    char cachekey[HOST_MAX_LEN + URI_MAX_LEN];  /* cache key */
    c_res_t *cacheres;          /* result obtained from cache */
    
    /* init struct req */
    req.fd = 0;
    strcpy(req.host, "");
    strcpy(req.method, "");
    strcpy(req.uri, "");
    strcpy(req.headers, "");
    strcpy(req.version, "");
    
    if (parse_req(connfd, &req) < 0) {
        /* parsing failed */
        err = BAD_REQUEST;
    }
    else {
        /* try cache first */
        strcpy(cachekey, req.host);
        strcat(cachekey, req.uri);
        /* cache hit */
        if ((cacheres = get(csh, cachekey))) {
            dbg_printf("Cache hit. Key: %s\n", cachekey);
            res = cacheres->val;
            reslen = cacheres->size;
            if (rio_writen(connfd, res, reslen) != reslen) {
                err = SERVER_ERROR;
                perror("Writing response - cached");
            }
            dbg_printf("Respond with cache. Key: %s\n", cachekey);
            /* malloced temporary results must be freed */
            free(cacheres);
        }
        /* cache miss */
        else {
            dbg_printf("%s %s %s\r\n%s", req.method, 
                        req.uri, req.version, req.headers);
            if ((responsefd = make_request(req)) < 0) {
                /* making request failed */
                err = SERVER_ERROR;
                perror("Make request error");
            }
            else {
                dbg_printf("Started consuming reponse from remote server.\n\n");
                
                if ((res = malloc(MAX_OBJECT_SIZE))) {
                    strcpy(res, "");
                }
                
                Rio_readinitb(&rio, responsefd);
                /* read from the real server */
                while ((readlen = rio_readnb(&rio, buf, MAX_OBJECT_SIZE)) > 0) {
                    /* cache only if not exceeding the object size limit */
                    if (res && reslen + readlen <= MAX_OBJECT_SIZE) {
                        memcpy(res + reslen, buf, readlen);
                    }
                    reslen += readlen;
                    if (rio_writen(connfd, buf, readlen) != readlen) {
                        err = SERVER_ERROR;
                        perror("Writing response");
                        break;
                    }
                }
                if (close(responsefd) < 0) {
                    perror("Close response fd");
                }
                
                /* error occurred when reading */
                if (readlen < 0) {
                    err = SERVER_ERROR;
                    perror("Reading response");
                    if (res) {
                        free(res);
                    }
                }
                
                /* eligible for caching */
                if (!err && res && reslen <= MAX_OBJECT_SIZE) {
                    res = realloc(res, reslen);
                    if (put(csh, cachekey, res, reslen) == 0) {
                        dbg_printf("Put cache succ. Key: %s, len: %d\n", 
                                    cachekey, reslen);
                    }
                    else {
                        dbg_printf("Put cache fail. Key: %s, len: %d\n", 
                                    cachekey, reslen);
                        free(res);
                    }
                }
                /* not eligible for caching, 
                 * but malloced temporary cache */
                else if (res) {
                    free(res);
                }
            }
        }
    }
    /* error ocurred during serving */
    if (err) {
        resp_error(err, connfd);
    }
    /* normal */
    else {
        if (close(connfd) < 0) {
            perror("Serve - close conn fd");
        }
    }
    
}

/*
 * New thread routine.
 */
static void *new_thread(void *fd) {
    /* obtain the passed conn fd */
    int connfd = *(int *)fd;
    pthread_detach(pthread_self());
    serve(connfd);
    
    free(fd);
    
    return NULL;
}

/*
 * Function for starting the server.
 */
static void run_server(char *port) {
    int *connfd;
    int tmpfd;
    
#ifdef DEBUG
    char clienthostname[MAXLINE], clientport[MAXLINE];
#endif

    struct sockaddr_storage sockaddr;
    socklen_t socklen;
    pthread_t tid;
    
    listenfd = Open_listenfd(port);
    
    /* init the cache */
    csh = init_cache(MAX_CACHE_SIZE);
    
    while (1) {
        socklen = sizeof(sockaddr);
        
        /* save the fd into a tmp so that nothing is malloced
         * if the server is shut down when waiting for connection.*/
        tmpfd = Accept(listenfd, (SA *) &sockaddr, &socklen);
        connfd = malloc(sizeof(int));
        *connfd = tmpfd;
        
#ifdef DEBUG
        Getnameinfo((SA *) &sockaddr, socklen, 
            clienthostname, MAXLINE, clientport, MAXLINE, 0);
        dbg_printf("Got connection from: %s:%s\n", clienthostname, clientport);
#endif

        if (pthread_create(&tid, NULL, new_thread, (void *)connfd) != 0) {
            resp_error(SERVER_ERROR, *connfd);
            free(connfd);
        }
    }
    
    if (close(listenfd) < 0) {
        perror("Run server - close listen fd");
    }
    
    /* free the cache */
    free_cache(csh);
}

int main(int argc, char **argv)
{
    if (argc < 2 || atoi(argv[1]) == 0) {
        usage();
    }
    
    /* set signal handlers */
    Signal(SIGINT,  sigint_handler);
    Signal(SIGTERM,  sigterm_handler);
    Signal(SIGPIPE,  sigpipe_handler);
    
    /* argv[1] is the port to listen on */
    run_server(argv[1]);
    
    return 0;
}
