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

/*
 * Defined indices for identify flags in the flag array
 * to check wether the request header contains the corresponding fields
 */
#define HOST             0
#define USER_AGENT       1
#define ACCEPT           2
#define ACCEPT_ENCODING  3
#define CONNECTION       4
#define PROXY_CONNECTION 5

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
    char protocal[MAXLINE];
    char resource[MAXLINE];
    char remote_host_name[MAXLINE], remote_host_port[MAXLINE];
    char uri_check[7];

    int flag[6];    // flag array to indentify request head settings
    for (int i = 0; i < 6; i++) {
        flag[i] = 0;
    }

    // read the request from the connfd
    Rio_readinitb(&rio, fd);
    if (Rio_readlineb(&rio, buf, MAXLINE) == -1) {
        printf("Null request.\n");
        return NULL;
    }

    // get the content of the request
    sscanf(buf, "%s %s %s", method, uri, version);

    // check whether the request method is legal (only implement GET)
    if (strcmp(method, "GET")) {
        if (Rio_writen(fd, method_error_str, strlen(method_error_str)) == -1) {
            printf("rio_writen error.\n");
            return;
        }
        printf("Not implemented. Sever only implements GET method.\n");
        return;
    }

    // check whether the request uri is legal
    strncpy(uri, uri_check, 7);
    if (strcmp(uri_check, "http://")) {
        if (Rio_writen(fd, uri_error_str, strlen(uri_error_str)) == -1) {
            printf("rio_writen error.\n");
            return;
        }
        printf("Not found. Invalid URI.\n");
        return;
    }

    parse_uri(uri, remote_host_name, remote_host_port, protocol, resource);

    // generate request line
    strcpy(request_buf, method);
    strcat(request_buf, " ");
    strcat(request_buf, resource);
    strcat(request_buf, " ");
    strcat(request_buf, version);
    strcat(request_buf, "\r\n");

    // generate request headers according to the client header
    while (Rio_readlineb(&rio, buf, MAXLINE) != 0) {

        generate_request_header(buf, request_buf, flag);

    }


}

static void generate_request_header(char* buf, char* request_header, int* flag) {

    char host_buf[MAXLINE];

    if (strcmp(buf, "\r\n") == 0) {
        return flag;
    } else {
        if (strstr(buf, "Host:") != NULL) {
            strcat(request_header, buf);
            flag[HOST] = 1;
        } else if (strstr(buf, "User-Agent:") != NULL) {
            strcat(request_header, user_agent_hdr);
            flag[USER_AGENT] = 1;
        } else if (strstr(buf, "Accept:") != NULL) {
            strcat(request_header, accept_str);
            flag[ACCEPT] = 1;
        } else if (strstr(buf, "Accept-Encoding:") != NULL) {
            strcat(request_header, accept_encoding_str);
            flag[ACCEPT_ENCODING] = 1;
        } else if (strstr(buf, "Connection:") != NULL) {
            strcat(request_header, connection_str);
            flag[CONNECTION] = 1;
        } else if (strstr(buf, "Proxy-Connection:") != NULL) {
            strcat(request_header, proxy_connection_str);
            flag[PROXY_CONNECTION] = 1;
        } else {
            strcat(request_header, buf);
        }

    }
    return flag;
}

/*
 * parse_uri - helper function to parse the fields in the request uri string
 */
static void parse_uri(char *uri, char *host_name, char *host_port, char *protocal, char *resource) {

    char host_name_port[MAXLINE];
    char *tmp;
    // check whether the uri contains a protocal
    if (strstr(uri, "://") != NULL) {
        // the uri contains a protocal
        sscanf(uri, "%[^:]://%[^/]%s", protocol, host_name_port, resource);
    } else {
        // the uri doesn't contain a protocal
        sscanf(uri, "%[^/]%s", host_name_port, resource);
    }

    // get the remote host name and remot host port
    tmp = strstr(host_name_port, ":");
    if (tmp != NULL) {
        // if there is a host port, cut the str to "name" "port"
        *tmp = "\0";
        // get the host port
        strcpy(host_port, tmp + 1);
    } else {
        // if there is no host port, set it as default 80
        strcpy(host_port, "80");
    }
    // set the host name
    strcpy(host_name_port, host_name);

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
