#include "proxy.h"

#define USECACHE 0

typedef struct
{
    int connfd;
    cache_list_t *cache_list;
} arg_t;

int main(int argc, char **argv)
{
    int listenfd;
    char hostname[MAXLINE], port[MAXLINE], *proxyport;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    cache_list_t cache_list = {.head = NULL, .len = 0, .new_ticket = 0};
    pthread_t tid;
    arg_t *argp;

    //  Signal 추가
    Signal(SIGPIPE, SIG_IGN);

    /* Check command line args */
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    proxyport = argv[1];
    printf("Proxy Server Start on port : %s\n", proxyport);
    listenfd = open_listenfd(proxyport);
    while (1)
    {
        clientlen = sizeof(clientaddr);
        argp = malloc(sizeof(arg_t));
        argp->connfd = accept(listenfd, (SA *)&clientaddr, &clientlen);
        argp->cache_list = &cache_list;
        getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);

        pthread_create(&tid, NULL, thread, argp);
    }

    return 0;
}

// Thread routine
void *thread(void *vargp)
{
    arg_t arg = *((arg_t *)vargp);
    int connfd = arg.connfd;
    cache_list_t *cache_list = arg.cache_list;
    pthread_detach(pthread_self());
    free(vargp);
    printf("Thread(%d) start\n", (int)pthread_self());
    run_proxy(connfd, cache_list);
    close(connfd);
    return NULL;
}

//write 하는 함수
void connect_node(cache_node_t *prev, cache_node_t *next)
{
    read_start(prev);
    read_start(next);

    if (!prev && !next)
        return;
    else if (!prev)
    {
        next->prev = NULL;
        return;
    }
    else if (!next)
    {
        prev->next = NULL;
        return;
    }
    else
    {
        next->prev = prev;
        prev->next = next;
        return;
    }

    read_end(prev);
    read_end(next);
}

void refresh_ticket(cache_list_t *cache_list)
{
    size_t new_ticket = 0;
    cache_node_t *node = cache_list->head;
    while (node)
    {
        read_start(node);
        node->ticket = new_ticket++;
        node = node->next;
        read_end(node);
    }
    cache_list->new_ticket = new_ticket;
}

cache_node_t *find_cache(cache_list_t *cache_list, char *request_header)
{
    cache_node_t *node = cache_list->head;
    cache_node_t *victim = node;
    while (node != NULL)
    {
        read_start(node);
        if (!strcmp(request_header, node->request_header))
        {
            // 사용되니까 ticket의 값을 높여줌
            node->ticket = cache_list->new_ticket++;
            return node;
        }
        if(victim->ticket > node->ticket){
            victim= node;
        }
        node = node->next;
        read_end(node);
    }
    cache_list->victim = victim;
    return NULL;
}

cache_node_t *caching(cache_list_t *cache_list, cache_node_t *new_node)
{
    if (cache_list->len == MAX_LIST_LEN)
    {
        cache_node_t *free_node = cache_list->victim;
        if(!free_node){
            free_node = cache_list->head;
        }
        read_start(free_node);
        connect_node(free_node->prev, free_node->next);
        if (cache_list->head == free_node)
        {
            cache_list->head = free_node->next;
        }
        read_end(free_node);
        write_start(free_node);
        free(free_node);
        write_end(free_node);
    }
    new_node->prev = NULL;
    sem_init(&(new_node->mutext),0, 1);
    sem_init(&(new_node->w),0, 1);
    new_node->ticket = cache_list->new_ticket++;
    connect_node(new_node, cache_list->head);
    cache_list->head = new_node;
    if(cache_list->len < MAX_LIST_LEN){
        while(cache_list->lock);
        cache_list->lock = 1;
        cache_list->len++;
        cache_list->lock = 0;
    }
    if (cache_list->new_ticket == MAX_TICKET)
    {
        refresh_ticket(cache_list);
    }
}

void run_proxy(int client_fd, cache_list_t *cache_list)
{
    char request_buf[MAXLINE], response_buf[MAXLINE], method[MAX_INPUT], uri[MAXLINE], version[MAX_INPUT];
    rio_t rio_client, rio_server;

    //  STEP 1 : request 읽기
    rio_readinitb(&rio_client, client_fd);

    if (!rio_readlineb(&rio_client, request_buf, MAXLINE)) //  한줄 읽어오기
        return;
    printf("%s", request_buf);
    sscanf(request_buf, "%s %s %s", method, uri, version); //   공백 단위로 분리해서 읽기

    if (strcasecmp(method, "GET")) // method가 GET인지 비교
    {                              // line:netp:doit:beginrequesterr
        clienterror(client_fd, method, "501", "Not Implemented",
                    "Proxy does not implement this method");
        return;
    }

    //  STEP 2 : uri parse
    char hostname[MAXLINE], port[MAX_INPUT], path[MAXLINE];
    char *p = strchr((uri + 5), ':');
    *p = '\0';
    strcpy(hostname, uri + 7);
    strcpy(port, p + 1);
    p = strchr(port, '/');
    *p = '\0';
    strcpy(path, p + 1);

    sprintf(request_buf, "%s /%s %s\r\n", method, path, "HTTP/1.0");
    cache_node_t *node;
    if (USECACHE)
    {
        // cache 구조체 생성 및 저장
        node = calloc(1, sizeof(cache_node_t));
        strcpy(node->request_header, request_buf);
    }

    cache_node_t *cached_node;
    // cache 찾아서 보내기
    if (USECACHE && (cached_node = find_cache(cache_list, node->request_header)))
    {
        read_start(cached_node);
        rio_writen(client_fd, cached_node->cached_response, cached_node->response_size);
        read_end(cached_node);
        free(node);
    }
    else // cache 되어있지 않으면 서버에 연결 요청
    {    //  STEP 3 : server 연결
        int server_fd = open_clientfd(hostname, port);
        ssize_t n;
        size_t content_length;

        rio_readinitb(&rio_server, server_fd);

        // STEP 4 : request header 전송
        rio_writen(server_fd, request_buf, strlen(request_buf));
        do
        {
            n = rio_readlineb(&rio_client, request_buf, MAXLINE);
            rio_writen(server_fd, request_buf, n);
        } while (strcmp(request_buf, "\r\n"));

        if (USECACHE)
        {
            p = node->cached_response;
        }

        // STEP 5 : response header 전송
        while (strcmp(response_buf, "\r\n"))
        {
            n = rio_readlineb(&rio_server, response_buf, MAXLINE);
            // cache 처리
            if (USECACHE)
            {
                strcpy(p, response_buf);
                p += n;
                node->response_size += n;
            }

            if (strstr(response_buf, "Content-length"))
            {
                content_length = atol((strchr(response_buf, ':') + 1));
            }
            rio_writen(client_fd, response_buf, n);
        }

        //  STEP 6 : response body 전송
        size_t remaining = content_length;
        bool cached = false;
        if (USECACHE)
        {
            cached = (node->response_size + content_length < MAX_OBJECT_SIZE);
        }
        while (remaining > 0)
        {
            size_t chunk = remaining < MAXLINE ? remaining : MAXLINE;
            n = rio_readnb(&rio_server, response_buf, chunk);
            if (n <= 0)
                break;
            rio_writen(client_fd, response_buf, n);
            if (cached)
            {
                strcpy(p, response_buf);
                p += n;
                node->response_size += n;
            }
            remaining -= n;
        }

        if (cached)
        {
            caching(cache_list, node);
        }
        else if (USECACHE)
        {
            free(node);
        }
        close(server_fd);
    }
}

/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor="
                 "ffffff"
                 ">\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    rio_writen(fd, buf, strlen(buf));
}
/* $end clienterror */

/*
 * read_requesthdrs - read HTTP request headers
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];

    //  STEP 1 : 한줄 씩 읽어오면서 빈 줄이 들어올 때 까지 print하고 스킵
    //  NOTE : header를 처리해야한다면 아래 Rio_radlineb 밑에 처리할 것
    do
    { // line:netp:readhdrs:checkterm
        rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    } while (strcmp(buf, "\r\n"));
    return;
}
/* $end read_requesthdrs */


void read_start(cache_node_t *node){
    // 읽기 끼리 공유하는 것 수정
    sem_wait(&(node->mutext));
        node->readcnt++;
        if(node->readcnt == 1){
            // 처음 읽기 시작하면 쓰지 못하도록 락
            sem_wait(&(node->w));
        }
    sem_post(&(node->mutext));
    //여기서 부터 읽기
}
void read_end(cache_node_t *node){
    // 읽기 끼리 공유하는 것 수정
    sem_wait(&(node->mutext));
        node->readcnt--;
        if(node->readcnt == 0){
            // 읽는 사람이 없으면 쓰기 락 해제
            sem_post(&(node->w));
        }
    sem_post(&(node->mutext));
}

extern inline void write_start(cache_node_t *node){
    sem_wait(&(node->w));
}
extern inline void write_end(cache_node_t *node){
    sem_post(&(node->w));
}