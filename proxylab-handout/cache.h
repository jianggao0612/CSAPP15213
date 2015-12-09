/*
 * Name: Gao Jiang
 * Andrew ID: gaoj
 *
 * cache.h - prototypes and definitions for cache.c
 */
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* Defined a struct representing the cache node in the cache list */
typedef struct cache_node_t {
    char* cache_id;
    char* cache_content;
    unsigned int cache_length;
    struct cache_node_t* next;
} cache_node_t;

typedef struct cache_list_t {
    struct cache_node_t* head;
    struct cache_node_t* rear;
    unsigned int unassigned_length;
    sem_t read_mutex, write_mutex;
} cache_list_t;

/* Defined function controling the proxy cache */
cache_list_t* init_cache_list();
cache_node_t* create_cache_node(char* cache_id, char* cache_content,
                                unsigned int length, cache_node_t* next);
int add_cache_node_to_rear(cache_list_t* list, cache_node_t* node);
cache_node_t* search_cache_node(cache_list_t* list, char* id);
int read_cache_list(cache_list_t* list, char* id, char* content);
int evict_cache_node(cache_list_t* list);
cache_node_t* delete_cache_node(cache_list_t* list, char* id);
void free_cache_node(cache_node_t* node);
