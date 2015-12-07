
/*
 * init_cache - initialize s cache list
 *              return a pointer to the cache_list
 */
cache_list_t* init_cache_list() {

    // create a cache list
    cache_list_t* cache_list = (cache_list_t *)malloc(sizeof(cache_list_t));

    // initialize the fields of cache list
    cache_list -> head = NULL;
    cache_list -> rear = NULL;
    cache_list -> unassigned_length = MAX_CACHE_SIZE;
    Sem_init(&cache_list -> read_mutex, 0, 1);
    Sem_init(&cache_list -> write_mutex, 0, 1);

    return cache_list;

}

/*
 * init_cache_node - initialize a cache node
 *                   return a pointer to the cache node
 */
cache_node_t* create_cache_node(char* cache_id, char* cache_content,
                              unsigned int length, cache_node_t* next) {

    // create a cache node
    cache_node_t* cache_node = (cache_node_t *)malloc(sizeof(cache_node_t));
    // check whether the cache node is created
    if (cache_node == NULL) {
        printf("Malloc cache node error\n");
        return NULL;
    }

    // initialize node id
    cache_node -> cache_id = (char *)malloc(sizeof(char) * (strlen(cache_id) + 1));
    // check whether malloc succeed
    if (cache_node -> cache_id == NULL) {
        printf("Create cache id error.\n");
        return NULL;
    }
    strcpy(cache_node -> cache_id, cache_id);

    // initialize node content
    cache_content = (char *)malloc(sizeof(char) * length);
    // check whether malloc succeed
    if (cache_node -> cache_content = NULL) {
        printf("Create cache content error.\n");
        return NULL;
    }
    memcpy(cache_node -> cache_content, cache_content, length);

    cache_node -> cache_length = length;
    cache_node -> next = next;

    return cache_node;
}

/*
 * add_cache_node_to_rear - add new cache node and move the recently accessed node to the rear
 *                          return 1 if successful; return 0 if error
 */
 int add_cache_node_to_rear(cache_list_t* list, cache_node_t* node) {

     // check whether the list is NULL
     if (list == NULL) {
         return 0;
     }

     // multi-thread write control
     P(&(list -> write_mutex));

     // check whether the cache size is enough for the new node
     while (list -> unassigned_length < node -> cache_length) {
         // if unassigend size is less than node size, evict according to lru
         evict_cache_node(list);
     }

     // add node to the cache list
     if (list -> head == NULL) {
         list -> head = node;
         list -> rear = node;
     } else {
         list -> rear -> next = node;
         list -> rear = node;
     }
     list -> unassigned_length -= cache_length;

     // unlock
     V(&(list -> write_mutex));

     return 1;
 }

 /*
  * search_cache_node - search cache node in the cache list according to given id
  *                     return cache node if found; return NULL if not
  */
 cache_node_t* search_cache_node(cache_list_t* list, char* id) {

     if (list == NULL) {
         return NULL;
     }

     P(&(list -> read_mutex));
     cache_node_t* node = list -> head;

     while (node != NULL) {
         if (strcmp(node -> cache_id, id) == 0) {
             V(&(list -> read_mutex));
             return node;
         }
         node = node -> next;
     }

     V(&(list -> read_mutex));
     return NULL;
 }

 /*
  * read_cache_list - read cache content from the node in cache list
  *                   and move the recently accessed node to list rear for lru
  *                   return 1 if succeed; return 0 if error
  */
 int read_cache_list(cache_list_t* list, char* id, char* content) {

     if (list == NULL) {
         return 0;
     }

     if (id == NULL) {
         printf("cache id error.\n");
         return 0;
     }

     // search for the node in cache list
     if ((node = search_cache_node(list, id)) == NULL) {
         // not found
         printf("cannot find the node in cache");
         return 0;
     } else {
         // found the node, access the cache content
         memcpy(content, node -> cache_content, node -> cache_length);
         // delete the node from current position
         node = delete_cache_node(list, id);
         // move the node to the rear of cache list
         add_cache_node_to_rear(list, node);
         return 1;
     }
 }
 /*
  * evict_cache_node - evict a cache node when the cache list is full according to lru
  *                    return 1 if succeed; return 0 if error
  */
 int evict_cache_node(cache_list_t* list) {

     // check whether the list is NULL
     if (list == NULL) {
         return 0;
     }

     if (list -> head == NULL) {
         return 0;
     }

     // write lock
     P(&(list -> write_mutex));

     // get the evicted node
     cache_node_t* evicted_node = list -> head;
     // evict the node
     list -> head = list -> head -> next;

     // update the list rear if necessary
     if (evicted_node == list -> rear) {
         list -> rear = NULL;
     }

     // update the list length
     list -> unassigned_length += evicted_node -> cache_length;

     // write unlock
     V(&(list -> write_mutex));

     // free the space for the cache node
     free_cache_node(evicted_node);

     return 1;

 }

 /*
  * delete_cache_node - delete a node from the cache list by the given id
  *                     return the deleted node if found; return NULL if Not
  */
 cache_node_t* delete_cache_node(cache_list_t* list, char* id) {

     if (list == NULL) {
         return NULL;
     }

     P(&(list -> write_mutex));
     cache_node_t* pre_node = NULL;
     cache_node_t* curr_node = list -> head;

     while (curr_node != NULL) {

         // if found, delete the node and return
         if (strcmp(curr_node -> cache_id, id) == 0) {
             // corner case
             if (curr_node == list -> head) {
                 list -> head = curr_node -> next;
             }

             if (pre_node != NULL) {
                 pre_node -> next = curr_node -> next;
             }

             // corner case
             if (curr_node == list -> rear) {
                 list -> rear = pre_node;
                 list -> rear -> next = NULL;
             }

             list -> unassigned_length += curr_node -> cache_length;
             V(&(list -> write_mutex));
             return curr_node;
         }

         pre_node = curr_node;
         curr_node = curr_node -> next;
     }

     V(&(list -> write_mutex));
     return NULL;

 }

 /*
  * free_cache_node - free the deleted/evicted node from the cache list
  */
 void free_cache_node(cache_node_t* node) {

     if (node == NULL) {
         return;
     }

     Free(node -> cache_id);
     Free(node -> cache_content);
     Free(node);
 }
