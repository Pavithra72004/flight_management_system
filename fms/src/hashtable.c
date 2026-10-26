#include "fms.h"
#include <stdlib.h>
#include <string.h>

/* A hash table = an ARRAY of buckets; each bucket is a LINKED LIST of nodes.
 * Looking up a key is O(1) on average instead of walking the whole list. */

static size_t hash_key(const char *key)
{
    size_t h = 5381U;
    while (*key != '\0') {
        h = h * 33U + (unsigned char)*key;
        key++;
    }
    return h;
}

HashTable *ht_create(size_t nbuckets)
{
    HashTable *h;

    if (nbuckets == 0) return NULL;
    h = calloc(1, sizeof *h);
    if (h == NULL) return NULL;
    h->buckets = calloc(nbuckets, sizeof *h->buckets);
    if (h->buckets == NULL) {
        free(h);
        return NULL;
    }
    h->nbuckets = nbuckets;
    return h;
}

FmsStatus ht_put(HashTable *h, const char *key, void *value)
{
    size_t    idx;
    HashNode *n;

    if (h == NULL || key == NULL || value == NULL) return FMS_ERR_NULL;
    if (key[0] == '\0' || strlen(key) >= KEY_MAX) return FMS_ERR_INVALID;
    idx = hash_key(key) % h->nbuckets;
    for (n = h->buckets[idx]; n != NULL; n = n->next) {
        if (strcmp(n->key, key) == 0) return FMS_ERR_EXISTS;
    }
    n = calloc(1, sizeof *n);
    if (n == NULL) return FMS_ERR_MEM;
    snprintf(n->key, sizeof n->key, "%s", key);
    n->value = value;
    n->next = h->buckets[idx];                  /* insert at head of the chain */
    h->buckets[idx] = n;
    h->count++;
    return FMS_OK;
}

void *ht_get(const HashTable *h, const char *key)
{
    size_t    idx;
    HashNode *n;

    if (h == NULL || key == NULL) return NULL;
    idx = hash_key(key) % h->nbuckets;
    for (n = h->buckets[idx]; n != NULL; n = n->next) {
        if (strcmp(n->key, key) == 0) return n->value;
    }
    return NULL;
}

FmsStatus ht_remove(HashTable *h, const char *key)
{
    size_t     idx;
    HashNode  *n;
    HashNode **link;                            /* pointer to the pointer we may change */

    if (h == NULL || key == NULL) return FMS_ERR_NULL;
    idx  = hash_key(key) % h->nbuckets;
    link = &h->buckets[idx];
    for (n = *link; n != NULL; n = *link) {
        if (strcmp(n->key, key) == 0) {
            *link = n->next;
            free(n);
            h->count--;
            return FMS_OK;
        }
        link = &n->next;
    }
    return FMS_ERR_NOT_FOUND;
}

/* explicit cleanup: frees the nodes and the table, NOT the stored values */
void ht_destroy(HashTable *h)
{
    size_t i;

    if (h == NULL) return;
    for (i = 0; i < h->nbuckets; i++) {
        HashNode *n = h->buckets[i];
        while (n != NULL) {
            HashNode *next = n->next;
            free(n);
            n = next;
        }
        h->buckets[i] = NULL;
    }
    free(h->buckets);
    h->buckets = NULL;
    free(h);
}
