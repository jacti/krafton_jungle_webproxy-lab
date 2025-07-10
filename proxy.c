#include "proxy.h"

#define USECACHE 1

cache_list_t global_cache_list = {.head = NULL, .len = 0, .new_ticket = 0, .victim = NULL};

int main(int argc, char **argv)
{
    int listenfd, *confdp;
    char hostname[MAXLINE], port[MAXLINE], *proxyport;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

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
        confdp = malloc(sizeof(int));
        *confdp = accept(listenfd, (SA *)&clientaddr, &clientlen);
        getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);

        pthread_create(&tid, NULL, thread, confdp);
    }

    return 0;
}

// Thread routine
void *thread(void *vargp)
{
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self());
    free(vargp);
    printf("Thread(%d) start\n", (int)pthread_self());
    run_proxy(connfd, &global_cache_list);
    close(connfd);
    return NULL;
}

// write 하는 함수
void connect_node(cache_node_t *prev, cache_node_t *next)
{
    pthread_rwlock_rdlock(&global_cache_list.rwlock);
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

    pthread_rwlock_unlock(&global_cache_list.rwlock);
}

void refresh_ticket(cache_list_t *cache_list)
{
    pthread_rwlock_wrlock(&cache_list->rwlock);
    size_t new_ticket = 0;
    cache_node_t *node = cache_list->head;
    while (node)
    {
        node->ticket = new_ticket++;
        node = node->next;
    }
    cache_list->new_ticket = new_ticket;
    pthread_rwlock_unlock(&cache_list->rwlock);
}

cache_node_t *find_cache(cache_list_t *cache_list, char *request_header)
{
    cache_node_t *node = cache_list->head;
    cache_node_t *victim = node;
    pthread_rwlock_rdlock(&cache_list->rwlock);
    while (node != NULL)
    {
        if (!strcmp(request_header, node->request_header))
        {
            // 사용되니까 ticket의 값을 높여줌
            node->ticket = cache_list->new_ticket++;
            return node;
        }
        if (victim->ticket > node->ticket)
        {
            victim = node;
        }
        node = node->next;
    }
    cache_list->victim = victim;
    pthread_rwlock_unlock(&cache_list->rwlock);
    return NULL;
}

cache_node_t *caching(cache_list_t *cache_list, cache_node_t *new_node)
{
    pthread_rwlock_wrlock(&cache_list->rwlock);
    if (cache_list->len == MAX_LIST_LEN)
    {
        cache_node_t *free_node = cache_list->victim;
        if (!free_node)
        {
            free_node = cache_list->head;
        }
        connect_node(free_node->prev, free_node->next);
        if (cache_list->head == free_node)
        {
            cache_list->head = free_node->next;
        }
        free(free_node);
    }
    new_node->prev = NULL;
    new_node->ticket = cache_list->new_ticket++;
    connect_node(new_node, cache_list->head);
    cache_list->head = new_node;
    if (cache_list->len < MAX_LIST_LEN)
    {
        cache_list->len++;
    }
    pthread_rwlock_unlock(&cache_list->rwlock);
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
        pthread_rwlock_rdlock(&cache_list->rwlock);
        rio_writen(client_fd, cached_node->cached_response, cached_node->response_size);
        free(node);
        pthread_rwlock_unlock(&cache_list->rwlock);
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