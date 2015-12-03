#include <stdio.h>
#include "csapp.h"

#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 \
(X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_str = "Accept: text/html,\
application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_str = "Accept-Encoding: gzip, deflate\r\n";
static const char *connection_str = "Connection: close\r\n";
static const char *proxy_connection_str = "Proxy-Connection: close\r\n";
static const char *get_str = "GET ";
static const char *version_str = " HTTP/1.0\r\n";
static const char *method_error_str = "Not implemented.\
Server only implements GET method.\r\n";
static const char *uri_error_str = "Not found. Invalid URI.\r\n";

int main(int argc, char **argv) {

    int listenfd, *connfd, port;
    struct sockaddr_in clientaddr;
    socklen_t clientlen;
    pthread_t tid;

    sem_t mutex;

    Signal(SIGPIPE, SIG_IGN);   // ignore SIGPIPE signal

    // check whether the input argument is legal
    if (argc != 2) {
        printf("Illegal Argument.\n");
        exit(0);
    }

    // check whether the input port is legal numbers
    if (!isValidPort(argv[1])) {
        printf("Illegal Port.\n");
        exit(0);
    }

    port = atoi(argv[1]);   // get the port number

    // check whether the port is within the legal range
    if ((port < 1000) || (port > 65535)) {
        printf("Illegal port.\n");
    }

    Sem_init(&mutex, 0, 1);
    listenfd = Open_listenfd(port);

    while (1) {

        clientlen = sizeof(struct sockaddr_in);
        connfd = Malloc(sizeof(int));
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        Pthread_create(&tid, NULL, thread, connfd);

    }

    return 0;

}

/*
 * thread - Thread routine
 */
void *thread(void *vargp) {

    int connfd = *((int *)vargp);
    Pthread_detach(pthead_self);
    Free(vargp);
    echo(connfd);   // main function for the proxy behavior
    Close(connfd);
    return NULL;

}

void echo(int fd) {
    rio_t rio;
    char buf[MAXLINE], req_buf[MAXLINE];
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char host_port[MAXLINE];
    char remote_host[MAXLINE], remote_port[MAXLINE];
    char uri_check[7];

    strcpy(remote_host, "");
    strcpy(remote_port, "80");

    Rio_readinitb(&rio, fd);
    if (Rio_readlineb(&rio, buf, MAXLINE) == -1) {
        printf("Null request.\n");
        return NULL;
    }

    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcmp(method, "GET")) {
        if (Rio_writen(fd, method_error_str, strlen(method_error_str)) == -1) {
            printf("rio_writen error.\n");
            return;
        }
        printf("Not implemented. Sever only implements GET method.\n");
        return;
    }

    strncpy(uri, uri_check, 7);
    if (strcmp(uri_check, "http://")) {
        if (Rio_writen(fd, uri_error_str, strlen(uri_error_str)) == -1) {
            printf("rio_writen error.\n");
            return;
        }
        printf("Not found. Invalid URI.\n");
        return;
    }

}



/*
 * isValidPort - Validate the given port is valid Number.
 *               return 1, if valid; return 0, if not.
 */
static int isValidPort(char *port) {

    int flag = 1;

    for (int i = 0; i < strlen(port); i++) {
        // determine each char is within the ASCII of 0-9
        if (*port < '0' || *port > '9') {
            flag = 0;
            break;
        }
        port++;

    }

    return flag;
}
