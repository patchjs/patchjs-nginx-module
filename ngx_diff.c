#include <ngx_core.h>
#include <ngx_md5.h>

#define CHUNK_SIZE 20;

typedef struct node {
	void *data;
	struct node *next;
} Node;

typedef struct linked_list {
	Node *head;
	Node *tail;
	Node *current;
} LinkedList;

void initialize_list(LinkedList *list) {
	list->head = NULL;
	list->tail = NULL;
	list->current = NULL;
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


Result* calc_diff_data(u_char* local_file_cnt, u_char* file_cnt){
	Result *result;
	result->m = !0;
	result->l = CHUNK_SIZE;
	LinkedList *diff_data_list = NULL;
	initialize_list(diff_data_list);
	if (ngx_strcmp(md5(local_file_cnt), md5(file_cnt)) == 0) {
		result->diff_data_list = diff_data_list;
		return result;
	}

}



u_char* md5(const u_char* cnt) {
	u_char result[16];
	ngx_md5_t ctx;
	ngx_md5_init(&ctx);
	ngx_md5_update(&ctx, cnt, ngx_strlen(cnt));
	ngx_md5_final(result, &ctx);
	return result;
}
