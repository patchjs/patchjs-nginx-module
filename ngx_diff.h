
#include <ngx_core.h>

typedef struct res{
	ngx_uint_t m;
	ngx_uint_t l;
	u_char *content;
} Res;

extern ngx_str_t* calc_diff_data(ngx_pool_t *pool, u_char* src_file_cnt, ngx_uint_t src_len, u_char* dst_file_cnt, ngx_uint_t dst_len);
