#include "hash_table.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* element of the hash table's chain list */
struct kv {
    struct kv* next;
    char* key;
    ngx_uint_t size;
    void* value;
    void(*free_value)(void*);
};

/* HashTable */
struct HashTable {
    struct kv ** table;
    ngx_pool_t *pool;
};

/* constructor of struct kv */
static void init_kv(struct kv* kv) {
    kv->next = NULL;
    kv->key = NULL;
    kv->value = NULL;
    kv->free_value = NULL;
}
/* destructor of struct kv */
static void free_kv(struct kv* kv) {
    if (kv) {
        if (kv->free_value) {
            kv->free_value(kv->value);
        }
        free(kv->key);
        kv->key = NULL;
        free(kv);
    }
}
/* the classic Times33 hash function */
static ngx_uint_t hash_33(char* key) {
    ngx_uint_t hash = 0;
    while (*key) {
        hash = (hash << 5) + hash + *key++;
    }
    return hash;
}

/* new a HashTable instance */
HashTable* hash_table_new(ngx_pool_t *pool, ngx_uint_t size) {
    HashTable* ht = ngx_palloc(pool, sizeof(HashTable)); 
    if (NULL == ht) {
        hash_table_delete(ht);
        return NULL;
    }
    ht->pool = pool;
    ht->size = size;
    ht->table = ngx_palloc(pool, sizeof(struct kv*) * ht->size);
    if (NULL == ht->table) {
        hash_table_delete(ht);
        return NULL;
    }
    ngx_memzero(ht->table, sizeof(struct kv*) * ht->size);

    return ht;
}

/* delete a HashTable instance */
void hash_table_delete(HashTable* ht) {
    if (ht) {
        if (ht->table) {
            ngx_uint_t i = 0;
            for (i = 0; i<ht->size; i++) {
                struct kv* p = ht->table[i];
                struct kv* q = NULL;
                while (p) {
                    q = p->next;
                    free_kv(p);
                    p = q;
                }
            }
            free(ht->table);
            ht->table = NULL;
        }
        free(ht);
    }
}

/* insert or update a value indexed by key */
ngx_int_t hash_table_put2(HashTable* ht, char* key, void* value, void(*free_value)(void*))
{
    ngx_int_t i = hash_33(key) % ht->size;
    struct kv* p = ht->table[i];
    struct kv* prep = p;

    while (p) { /* if key is already stroed, update its value */
        if (ngx_strcmp(p->key, key) == 0) {
            if (p->free_value) {
                p->free_value(p->value);
            }
            p->value = value;
            p->free_value = free_value;
            break;
        }
        prep = p;
        p = p->next;
    }

    if (p == NULL) {/* if key has not been stored, then add it */
        u_char* kstr = ngx_palloc(ht->pool, ngx_strlen(key) + 1);
        if (kstr == NULL) {
            return -1;
        }
        struct kv * kv = ngx_palloc(ht->pool, sizeof(struct kv));
        if (NULL == kv) {
            free(kstr);
            kstr = NULL;
            return -1;
        }
        init_kv(kv);
        kv->next = NULL;
        ngx_cpymem(kstr, key, ngx_strlen(key));
        kv->key = kstr;
        kv->value = value;
        kv->free_value = free_value;

        if (prep == NULL) {
            ht->table[i] = kv;
        }
        else {
            prep->next = kv;
        }
    }
    return 0;
}

/* get a value indexed by key */
void* hash_table_get(HashTable* ht, char* key)
{
    ngx_int_t i = hash_33(key) % ht->size;
    struct kv* p = ht->table[i];
    while (p) {
        if (ngx_strcmp(key, p->key) == 0) {
            return p->value;
        }
        p = p->next;
    }
    return NULL;
}

/* remove a value indexed by key */
void hash_table_rm(HashTable* ht, char* key)
{
    ngx_int_t i = hash_33(key) % ht->size;

    struct kv* p = ht->table[i];
    struct kv* prep = p;
    while (p) {
        if (ngx_strcmp(key, p->key) == 0) {
            free_kv(p);
            if (p == prep) {
                ht->table[i] = NULL;
            }
            else {
                prep->next = p->next;
            }
        }
        prep = p;
        p = p->next;
    }
}