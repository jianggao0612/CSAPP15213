#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* Defined a struct representing the cache node in the cache list */
typedef struct _cache_node_t {
    char* cache_id;
    char* cache_content;
    usigned int cache_length;
    struct cache_node_t* next;
} cache_node_t;

typedef struct _cache_list_t {
    struct cache_node_t* head;
    struct cache_node_t* rear;
    unsigned int unassigned_length;
    sem_t read_mutex, write_mutex;
} cache_list_t;
