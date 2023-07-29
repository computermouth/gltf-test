#ifndef SRC_INTERNAL_H_
#define SRC_INTERNAL_H_

#include "list.h"

list_node* _list_node_init(size_t type_size, const void *value);
void _list_node_free(list_node *n);

#include "map.h"

#define NBUCKETS 512
#define ALLOC_FACTOR 2.0
#define LOAD_FACTOR 1.0

void _map_free_buckets(map *m);
list* _map_find_bucket(const map *m, const void *key);
list_node* _map_find_bucket_node(const map *m, const list *bucket, const void *key);

#endif // SRC_INTERNAL_H_