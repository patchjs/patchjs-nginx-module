#include <ngx_core.h>
#include <ngx_md5.h>
#include "hash_table.h"

#define CHUNK_SIZE 20;

extern void calc_hash_table(HashTable *ht, u_char* file_cnt, ngx_uint_t len);

typedef struct node {
	void *data;
	struct node *next;
} Node;

typedef struct linked_list {
	Node *head;
	Node *tail;
	Node *current;
} LinkedList;

void initialize_list(LinkedList **list) {
	*list = (LinkedList*)malloc(sizeof(LinkedList));
	*list->head = NULL;
	*list->tail = NULL;
	*list->current = NULL;
}

void add_tail(LinkedList *list, void* data) {
	Node *node = (Node *) malloc(sizeof(Node));
	node->data = data;
	node->next = NULL;
	if (list->head == NULL) {
		list->head = node;
	} else {
		list->tail->next = node;
	}
	list->tail = node;
}

typedef struct result {
	ngx_int_t m;
	ngx_int_t l;
	LinkedList *diff_data_list;
	HashTable *ht_dst;
	HashTable *ht_src;
} Result;


u_char* substring(const u_char* src, ngx_int_t start, ngx_int_t end) {
	ngx_int_t len = end - start + 1;
	u_char *dest = malloc(len + 1);
	ngx_int_t k = 0,  = start;
	for (; i <= end; i++) {
		*(dest + k) = *(src + i);
		k++;
	}
	*(dest + k) = '\0';
	return dest;
}

Result* calc_diff_data(u_char* src_file_cnt, ngx_uint_t src_len, u_char* dst_file_cnt, ngx_uint_t dst_len){
	Result *result = (Result*) malloc(sizeof(Result));
	// result->ht_dst = 
	hash_table_new(result->ht_dst);
	hash_table_new(result->ht_src);
	result->m = false;
	result->l = CHUNK_SIZE;
	LinkedList *diff_data_list = NULL;
	initialize_list(diff_data_list);
	if (ngx_strcmp(md5(local_file_cnt), md5(file_cnt)) == 0) {
		result->diff_data_list = diff_data_list;
		return result;
	}

	calc_hash_table(result->ht_dst, dst_file_cnt, dst_len); // calc remote file context hash table

}

u_char* md5(const u_char* cnt) {
	u_char result[16];
	ngx_md5_t ctx;
	ngx_md5_init(&ctx);
	ngx_md5_update(&ctx, cnt, ngx_strlen(cnt));
	ngx_md5_final(result, &ctx);
	return result;
}

void calc_hash_table(HashTable *ht, u_char* file_cnt, ngx_uint_t len) 
{
	u_char *p = file_cnt;
	ngx_uint_t key_index = 0;
	for (int i=0; i<len; i+=CHUNK_SIZE) {
		char key[32];
		u_char chunk[CHUNK_SIZE] = {0};

		ngx_uint_t get_size = CHUNK_SIZE;
		if (len - i < CHUNK_SIZE) {
			get_size = CHUNK_SIZE - i;
		}

		ngx_strncpy(chunk, p, get_size);
        sprintf(key, "%d", key_index);
		hash_table_put(ht, key, md5(chunk))

		key_index++;
		p += CHUNK_SIZE;
	}
}
