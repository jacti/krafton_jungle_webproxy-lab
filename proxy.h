#include <stdbool.h>
#include "include/csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define MAX_LIST_LEN ((size_t)(MAX_CACHE_SIZE / MAX_OBJECT_SIZE))
#define MAX_TICKET 100

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

typedef struct
_cache_node{
    //key
    char request_header[MAXLINE];
    size_t response_size;
    char cached_response[MAX_OBJECT_SIZE];

    size_t ticket;
    struct _cache_node* prev;
    struct _cache_node* next;

} cache_node_t;

typedef struct{
    cache_node_t *head;
    size_t len;
    size_t new_ticket;
} cache_list_t;

cache_node_t *find_cache(cache_list_t*, char *);
cache_node_t *caching(cache_list_t *cache_list, cache_node_t* new_node);

void run_proxy(int, cache_list_t*);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void read_requesthdrs(rio_t *rp);
void refresh_ticket(cache_list_t *cache_list);