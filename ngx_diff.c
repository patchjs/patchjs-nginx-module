#include <ngx_md5.h>
#include "hash_table.h"

#define CHUNK_SIZE 20
#define MAX_CONTENT_SIZE (2*1024*1024)

ngx_pool_t *g_pool = NULL;

extern static void checksum(HashTable *ht, u_char* file_cnt, ngx_uint_t len);
extern static void roll(HashTable *ht, ngx_array_t diff_array, u_char* file_cnt, ngx_uint_t len);
extern static ngx_int_t find_match_order_id(HashTable *ht, u_char *md5_result, ngx_int_t last_order_id);

typedef struct diff_data{
	bool match;
	ngx_int_t order_id;
	u_char *start;
	ngx_uint_t len;
} DiffData;

typedef struct result {
	ngx_int_t m;
	ngx_int_t l;
	u_char *content;

	HashTable *ht_dst;
	HashTable *ht_src;

	ngx_pool_t *pool;
	ngx_array_t *diff_array;
} Result;

u_char* calc_diff_data(u_char* src_file_cnt, ngx_uint_t src_len, u_char* dst_file_cnt, ngx_uint_t dst_len){
	g_pool = ngx_pool_create(1024);
	Result *result = ngx_palloc(g_pool, sizeof(Result));
	result->pool = g_pool;
	result->m = true;
	result->l = CHUNK_SIZE;

	result->ht_dst = hash_table_new(result->pool);
	result->ht_src = hash_table_new(result->pool);

	if (ngx_strcmp(md5(local_file_cnt), md5(file_cnt)) == 0) {
		result->m = false;
		return result;
	}

	result->diff_array = (ngx_array_t*)ngx_array_create(ht->pool, 16, sizeof(DiffData));

	checksum(result->ht_dst, dst_file_cnt, dst_len); // calc remote file context hash table
	roll(result->ht_dst, result->diff_array, src_file_cnt, src_len); // calc remote file context hash table

	result->content = (u_char*)ngx_palloc(pool, sizeof(u_char)*MAX_CONTENT_SIZE);
	ngx_memzero(result->content, sizeof(u_char)*MAX_CONTENT_SIZE);
	u_char *p_content = result->content;

	DiffData *last_item = NULL;
	ngx_uint_t match_count = 0;
	DiffData *p = result->diff_array->elts;
	for (int i = 0, size = result->diff_array->nelts; i < size; i++) {
		DiffData *item = p[i];
		u_char temp[16] = {0};
		ngx_uint_t _len = 0;

		if (item->match) {
			if (last_item == NULL || !last_item->match) {
				ngx_snprintf(temp, 16, "[%d,", item->order_id);
				_len = ngx_strlen(temp);
				ngx_memcpy(p_content, temp, _len);
				p_content += _len;
				match_count = 1;
			} else if (last_item->match && (last_item->order_id+1) == item->order_id) {
				match_count++;
			} else if (last_item->match && (last_item->order_id+1) != item->order_id) {
				ngx_snprintf(temp, "%d]", match_count);
				_len = ngx_strlen(temp);
				ngx_memcpy(p_content, temp, _len);
				p_content += _len;
				match_count = 1;
			}

			if (i == size - 1) {
				ngx_snprintf(temp, "%d]", match_count);
				_len = ngx_strlen(temp);
				ngx_memcpy(p_content, temp, _len);
				p_content += _len;
			}
		} else {
			if (match_count > 0) {
				ngx_snprintf(temp, "%d]", match_count);
				_len = ngx_strlen(temp);
				ngx_memcpy(p_content, temp, ngx_strlen(temp));
				p_content += _len;
				match_count = 0;
			}
			p_content = ngx_memcpy(p_content, item->data, item->len);
			p_content += item->len;
		}
		last_item = item;
	}
	return result->content;
}

// 计算md5 不能改变cnt的内容const
u_char* md5(const u_char* const cnt, ngx_uint_t len) {
	u_char result[16];
	ngx_md5_t ctx;
	ngx_md5_init(&ctx);
	ngx_md5_update(&ctx, cnt, len);
	ngx_md5_final(result, &ctx);
	return result;
}

static void checksum(HashTable *ht, u_char* file_cnt, ngx_uint_t len)
{
	u_char *p = file_cnt;
	ngx_uint_t order_id = 0;
	for (int i=0; i<len; ) {
		ngx_uint_t get_size = CHUNK_SIZE;
		ngx_array_t *order_ids = NULL;
		ngx_uint_t *p_order_id = NULL;

		if (len - i < CHUNK_SIZE) {
			get_size = CHUNK_SIZE - i;
		}	

		u_char *md5_value = md5(p, get_size);
		order_ids = (ngx_uint_t *)hash_table_get(ht, md5_value);
		if (order_ids == NULL) {
			order_ids = ngx_array_create(ht->pool, 4, sizeof(ngx_uint_t));
			p_order_id = ngx_array_push(order_ids); // 添加一个元素
			*p_order_id = order_id++;
			hash_table_put(ht, md5_value);
		} else {
			p_order_id = ngx_array_push(chunk_data.order_ids); // 添加一个元素
			*p_order_id = order_id++;
		}

		p += get_size;
		i += get_size;
	}
}

static void roll(HashTable *ht, ngx_array_t diff_array, u_char* file_cnt, ngx_uint_t len)
{
	u_char *p = file_cnt; // 本地文件
	ngx_uint_t order_id = 1; // 初始chunk id

	u_char *unmatch_start = file_cnt; // 指向开始不匹配的的地址
	ngx_uint_t unmatch_len = 0; // 不匹配的长度

	ngx_int_t last_order_id = -1; // 

	for (int i=0; i<len; ) {
		ngx_int_t m_order_id = -1;

        // 或许chunk_size 剩余长度不足一个chunk大小 则有多少取多少
		ngx_uint_t get_size = CHUNK_SIZE;
		if (len - i < CHUNK_SIZE) {
			get_size = CHUNK_SIZE - i;
		}

        // 在目标文件的hash table中找此chunk的md5, 如果找到了p_order_id不为NULL，反之亦然
		u_char *md5_result = md5(p, get_size);
		match_order_id = find_match_order_id(ht, md5_result, last_order_id);
		if (match_order_id == -1) { // 未找到
			i++;
			unmatch_len++;
			p++;
		} else { // 已找到
			// ngx_uint_t match_order_id = int(m_order_id); // 目标文件的chunk ID    类型转换 void * 转化为 int
			DiffData *match_diff = NULL;
			DiffData *unmatch_diff = NULL;

            //  unmatch_size大于0的话，说明匹配chunk成功之前有不能匹配的数据, 所以先把不匹配的数据组成一个不定长度的chunk，再对匹配的chunk做记录
			if (unmatch_len > 0) {
				unmatch_diff = ngx_array_push(diff_array); // 添加一个元素
				unmatch_diff.match = false;
				unmatch_diff.order_id = -1;
				unmatch_diff.start = unmatch_start;
				unmatch_diff.len = unmatch_len;
			}

			match_diff = ngx_array_push(diff_array); // 添加一个元素
			match_diff.match = true;
			match_diff.order_id = match_order_id;
			match_diff.start = NULL;
			match_diff.len = 0;

			p += get_size;
			i += get_size;

			unmatch_start = p;
			unmatch_len = 0;
			last_order_id = match_order_id;
		}
	}	
}

static ngx_int_t find_match_order_id(HashTable *ht, u_char *md5_result, ngx_int_t last_order_id)
{
	ngx_array_t *value = (ngx_array_t*)hash_table_get(ht, md5_result);
	if (value == NULL) {
		return -1;
	}

	ngx_uint_t *order_ids = value->elts;
	if (value->nelts == 1) {
		return order_ids[0];
	} else {
		ngx_uint_t last_id = order_ids[0];
		ngx_uint_t result_id = 0;
		for (int i=0; i<value.nelts; i++) {
			ngx_uint_t id = order_ids[0];
			if (id >= last_order_id && last_id <= last_order_id) {
				return (last_order_id - last_id) >= (id - last_order_id) ? id : last_id;
			} else if (id >= last_order_id && last_id <= last_order_id) {
				return last_id;
			} else if (id <= last_order_id && last_id <= last_order_id) {
				return id;
			} else {
				result_id = id;
			}
		}
		return result_id;
	}

	return order_ids[0];
}

void cleanup_pool() 
{
	ngx_destroy_pool(g_pool);
}

