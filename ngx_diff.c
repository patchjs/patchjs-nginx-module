#include <ngx_md5.h>
#include <ngx_core.h>
#include "hash_table.h"

#define CHUNK_SIZE 20;

extern void calc_hash_table(HashTable *ht, u_char* file_cnt, ngx_uint_t len);
extern void calc_diff_data_result(HashTable *ht, u_char* file_cnt, ngx_uint_t len)

typedef struct node {
	ngx_uint_t order_id; 	// 新chunk ID
	ngx_uint_t match_order_id; 	// 匹配chunk ID
	u_char is_exist; 		// 是否存在
	void *data;				// 不存在 则有数据
	ngx_uint_t len;			// 数据长度
	struct node *next;
} Node;

typedef struct linked_list {
	Node *head;
	Node *tail;
	// Node *current;
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
	result->m = true;
	result->l = CHUNK_SIZE;
	LinkedList *diff_data_list = NULL;
	initialize_list(diff_data_list);
	if (ngx_strcmp(md5(local_file_cnt), md5(file_cnt)) == 0) {
		result->m = false;
		result->diff_data_list = diff_data_list;
		return result;
	}

	calc_hash_table(result->ht_dst, dst_file_cnt, dst_len); // calc remote file context hash table
	calc_diff_data_result(result->ht_dst, src_file_cnt, src_len); // calc remote file context hash table

	return result;
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
	ngx_uint_t order_id = 1;
	for (int i=0; i<len; ) {
		// char key[32];
		u_char chunk[CHUNK_SIZE] = {0};

		ngx_uint_t get_size = CHUNK_SIZE;
		if (len - i < CHUNK_SIZE) {
			get_size = CHUNK_SIZE - i;
		}

		ngx_strncpy(chunk, p, get_size);
        // sprintf(key, "%d", order_id);
		hash_table_put(ht, md5(chunk), order_id++)

		p += get_size;
		i += get_size;
	}
}

void calc_diff_data_result(HashTable *ht, u_char* file_cnt, ngx_uint_t len)
{
	u_char *p = file_cnt; // 本地文件
	ngx_uint_t order_id = 1; // 初始chunk id

	u_char *unmatch_start = file_cnt; // 指向开始不匹配的的地址
	ngx_uint_t unmatch_size = 0; // 不匹配的长度

	for (int i=0; i<len; ) {
		char key[32];
		u_char chunk[CHUNK_SIZE] = {0};

        // 或许chunk_size 剩余长度不足一个chunk大小 则有多少取多少
		ngx_uint_t get_size = CHUNK_SIZE;
		if (len - i < CHUNK_SIZE) {
			get_size = CHUNK_SIZE - i;
		}

        // 拷贝一个chunk 并计算此chunk的md5
		ngx_strncpy(chunk, p, get_size);
		u_char *md5_result = md5(chunk);
        
        // 在目标文件的hash table中找此chunk的md5, 如果找到了p_order_id不为NULL，反之亦然
		void *p_order_id = hash_table_get(ht, md5_result);
		if (p_order_id == NULL) { // 未找到
			i++;
			unmatch_size++;
			p++;
		} else { // 已找到
			ngx_uint_t match_order_id = int(p_order_id); // 目标文件的chunk ID    类型转换 void * 转化为 int

            //  unmatch_size大于0的话，说明匹配chunk成功之前有不能匹配的数据, 所以先把不匹配的数据组成一个不定长度的chunk，再对匹配的chunk做记录
			if (unmatch_size > 0) {
				Node *pNode = (Node*) malloc(sizeof(Node));
				pNode->is_exist = 0;
				pNode->order_id = order_id++;
				pNode->len = unmatch_size; // 不匹配的数据长度
				pNode->data = (u_char *)malloc(sizeof(u_char) * unmatch_size); 
				ngx_strncpy(pNode->data, unmatch_start, unmatch_size); // 记录数据

				ht->list->tail->next = pNode;
				pNode->next = ht->list->head;
				ht->list->tail = pNode;

				if (ht->list->head == NULL) {
					ht->list->head = pNode
				}

			}

            // 匹配的chunk
			Node *pNode = (Node *)malloc(sizeof(Node));
			pNode->is_exist = 1; // 已存在
			pNode->order_id = order_id++;
			pNode->match_order_id = match_order_id; // 匹配的chunk ID
			pNode->data = NULL; // 数据不记录

			ht->list->tail->next = pNode;
			pNode->next = ht->list->head;
			ht->list->tail = pNode;

			if (ht->list->head == NULL) {
				ht->list->head = pNode
			}

			p += get_size;
			i += get_size;

			unmatch_start = p;
			unmatch_size = 0;
		}
	}	
}

