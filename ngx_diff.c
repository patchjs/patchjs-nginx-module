#include <ngx_md5.h>
#include "ngx_diff.h"
#include "hashmap.h"

#define CHUNK_SIZE 20
#define MD5_CONTENT_SIZE 16

ngx_pool_t *g_pool = NULL;

static void checksum(ngx_http_request_t *r, map_t ht, u_char* file_cnt, ngx_uint_t len);
static ngx_uint_t roll(map_t ht, ngx_array_t *diff_array, u_char* file_cnt, ngx_uint_t len);
static ngx_int_t find_match_order_id(map_t ht, u_char *md5_result, ngx_uint_t last_order_id);
static u_char* make_md5(const u_char* cnt, ngx_uint_t len, u_char *result);

typedef struct diff_data{
	u_char match;
	ngx_int_t order_id;
	u_char *start;
	ngx_uint_t len;
} DiffData;

typedef struct result {
	map_t ht_dst;

	ngx_pool_t *pool;
	ngx_array_t *diff_array;
} Result;

ngx_str_t * calc_diff_data(ngx_http_request_t *r, u_char* src_file_cnt, ngx_uint_t src_len, u_char* dst_file_cnt, ngx_uint_t dst_len) {
	ngx_str_t *res = (ngx_str_t *)ngx_palloc(r->pool, sizeof(ngx_str_t));
	u_char prefix[32] = {0};
	ngx_str_t tail = ngx_string("]}");
	ngx_uint_t move_len = 0;

	Result *result = ngx_palloc(r->pool, sizeof(Result));
	ngx_pool_t *pool = r->pool;
	result->pool = pool;

	result->ht_dst = hashmap_new(pool, MD5_CONTENT_SIZE);

	ngx_memzero(prefix, sizeof(prefix));
	u_char src_md5[17], dst_md5[17];
	ngx_memzero(src_md5, sizeof(src_md5));
	ngx_memzero(dst_md5, sizeof(dst_md5));
	make_md5(src_file_cnt, src_len, src_md5);
	make_md5(dst_file_cnt, dst_len, dst_md5);

	if (ngx_strncmp(src_md5, dst_md5, sizeof(src_md5)) == 0) {
		ngx_sprintf(prefix, "{\"m\":false,\"l\":%d,\"c\":[]}", CHUNK_SIZE);
		res->len = ngx_strlen(prefix);
		res->data = (u_char*)ngx_palloc(pool, sizeof(u_char) * res->len);
		ngx_memcpy(res->data, prefix, res->len);

		return res;
	}

	result->diff_array = (ngx_array_t*)ngx_array_create(pool, 16, sizeof(DiffData));
	checksum(r, result->ht_dst, dst_file_cnt, dst_len); /* calc remote file context hash table */
	roll(result->ht_dst, result->diff_array, src_file_cnt, src_len); /* calc remote file context hash table */

	res->len = src_len + 32;
	res->data = (u_char*)ngx_palloc(pool, res->len);
	ngx_memzero(res->data, sizeof(u_char)*res->len);
	u_char *p_content = res->data;

	/* head */
	ngx_sprintf(prefix, "{\"m\":true,\"l\":%d,\"c\":[", CHUNK_SIZE);
	move_len = ngx_strlen(prefix);
	ngx_memcpy(p_content, prefix, move_len);
	p_content += move_len;
	u_char *p_content_start = p_content;

	DiffData *last_item = NULL;
	ngx_uint_t match_count = 0;
	DiffData *p = (DiffData *)result->diff_array->elts;
	for (int i = 0, size = result->diff_array->nelts; i < size; i++) {
		DiffData *item = p + i;
		u_char temp[16] = {0};
		ngx_memzero(temp, sizeof(temp));
		ngx_uint_t _len = 0;

		if (item->match) {
			if (last_item == NULL || !last_item->match) {
				if (p_content_start != p_content) {
					*p_content++ = ',';
				}
				ngx_snprintf(temp, 16, "[%d,", item->order_id);
				_len = ngx_strlen(temp);
				ngx_memcpy(p_content, temp, _len);
				p_content += _len;
				match_count = 1;
			} else if (last_item->match && (last_item->order_id+1) == item->order_id) {
				match_count++;
			} else if (last_item->match && (last_item->order_id+1) != item->order_id) {
				ngx_snprintf(temp, 16, "%d]", match_count);
				_len = ngx_strlen(temp);
				ngx_memcpy(p_content, temp, _len);
				p_content += _len;

				ngx_memzero(temp, sizeof(temp));
				ngx_snprintf(temp, 16, ",[%d,", item->order_id);
				_len = ngx_strlen(temp);
				ngx_memcpy(p_content, temp, _len);
				p_content += _len;

				match_count = 1;
			}

			if (i == size - 1) {
				ngx_memzero(temp, sizeof(temp));
				ngx_snprintf(temp, 16, "%d]", match_count);
				_len = ngx_strlen(temp);
				ngx_memcpy(p_content, temp, _len);
				p_content += _len;
			}
		} else {
			if (match_count > 0) {
				ngx_snprintf(temp, 16, "%d]", match_count);
				_len = ngx_strlen(temp);
				ngx_memcpy(p_content, temp, ngx_strlen(temp));
				p_content += _len;
				match_count = 0;
			} 

			if (p_content_start != p_content) {
				*p_content++ = ',';
			}
			*p_content++ = '"';
			ngx_memcpy(p_content, item->start, item->len);
			p_content += item->len;
			*p_content++ = '"';
		}
		last_item = item;
	}

	/* tail */
	ngx_memcpy(p_content, tail.data, tail.len);
	p_content += tail.len;
	res->len = p_content - res->data;

	return res;
}

/* calc md5 */ 
static u_char* make_md5(const u_char* cnt, ngx_uint_t len, u_char *result) {
	ngx_md5_t ctx;
	ngx_md5_init(&ctx);
	ngx_md5_update(&ctx, cnt, len);
	ngx_md5_final(result, &ctx);
	return result;
}

static void checksum(ngx_http_request_t *r, map_t ht, u_char* file_cnt, ngx_uint_t len) {
	ngx_pool_t *pool = r->pool;
	u_char *p = file_cnt;
	ngx_uint_t order_id = 0;
	for (ngx_uint_t i=0; i<len; ) {
		ngx_uint_t get_size = CHUNK_SIZE;
		ngx_array_t *order_ids = NULL;
		ngx_uint_t *p_order_id = NULL;
		int ret = -1;

		if (len - i < CHUNK_SIZE) {
			get_size = len - i;
		}	

		u_char *md5_value = ngx_palloc(pool, MD5_CONTENT_SIZE+1);
		ngx_memzero(md5_value, MD5_CONTENT_SIZE+1);
		make_md5(p, get_size, md5_value);

		ret = hashmap_get(ht, (char*)md5_value, MD5_CONTENT_SIZE, (void**)(&order_ids));
		if (ret == MAP_MISSING) {
			order_ids = ngx_array_create(pool, 4, sizeof(ngx_uint_t));
			p_order_id = ngx_array_push(order_ids); 
			*p_order_id = order_id++;

			hashmap_put(ht, (char *)md5_value, MD5_CONTENT_SIZE, order_ids);
		} else {
			p_order_id = ngx_array_push(order_ids); 
			*p_order_id = order_id++;
		}

		p += get_size;
		i += get_size;
	}
}

static ngx_uint_t roll(map_t ht, ngx_array_t *diff_array, u_char* file_cnt, ngx_uint_t len) {
	u_char *p = file_cnt; /* local file */

	u_char *unmatch_start = file_cnt; /* point unmatch addr */
	ngx_uint_t unmatch_len = 0; /* unmatch length */

	ngx_int_t last_order_id = -1;  
	bool is_last = false;

	for (ngx_uint_t i=0; i<len; ) {
		DiffData *match_diff = NULL;
		DiffData *unmatch_diff = NULL;

		ngx_uint_t get_size = CHUNK_SIZE;
		if (len - i <= CHUNK_SIZE) {
			get_size = len - i;
			is_last = true;
		}

		u_char md5_result[MD5_CONTENT_SIZE+1] = {0};
		ngx_memzero(md5_result, MD5_CONTENT_SIZE+1);
		ngx_int_t match_order_id = -1;
		if (!is_last) {
			make_md5(p, get_size, md5_result);
			match_order_id = find_match_order_id(ht, md5_result, last_order_id);
		}

		if (match_order_id == -1) { /* no found */
			i++;
			unmatch_len++;
			p++;
		} else { 
			if (unmatch_len > 0) {
				unmatch_diff = ngx_array_push(diff_array); 
				unmatch_diff->match = 0;
				unmatch_diff->order_id = -1;
				unmatch_diff->start = unmatch_start;
				unmatch_diff->len = unmatch_len;
			}

			match_diff = ngx_array_push(diff_array); 
			match_diff->match = 1;
			match_diff->order_id = match_order_id;
			match_diff->start = NULL;
			match_diff->len = 0;

			p += get_size;
			i += get_size;

			unmatch_start = p;
			unmatch_len = 0;
			last_order_id = match_order_id;
		}

		if (i == len) {
			if (unmatch_len > 0) {
				unmatch_diff = ngx_array_push(diff_array); 
				unmatch_diff->match = 0;
				unmatch_diff->order_id = -1;
				unmatch_diff->start = unmatch_start;
				unmatch_diff->len = unmatch_len;
			}
		}
	}

	return 0;
}

static ngx_int_t find_match_order_id(map_t ht, u_char *md5_result, ngx_uint_t last_order_id)
{
	ngx_array_t *value = NULL; 
	int ret = hashmap_get(ht, (char *)md5_result, MD5_CONTENT_SIZE, (void**)(&value));
	if (ret == MAP_MISSING) {
		return -1;
	}

	ngx_uint_t *order_ids = value->elts;
	if (value->nelts == 1) {
		return order_ids[0];
	} else {
		ngx_uint_t last_id = order_ids[0];
		ngx_uint_t result_id = 0;
		for (ngx_uint_t i=0; i<value->nelts; i++) {
			ngx_uint_t id = order_ids[i];
			if (id >= last_order_id && last_id <= last_order_id) {
				return (last_order_id - last_id) >= (id - last_order_id) ? id : last_id;
			} else if (id >= last_order_id && last_id >= last_order_id) {
				return last_id;
			} else if (id <= last_order_id && last_id <= last_order_id) {
				result_id = id;
			} else {
				result_id = id;
			}
			last_id = id;
		}
		return result_id;
	}

	return order_ids[0];
}