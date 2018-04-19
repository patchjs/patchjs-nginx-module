
#include <ngx_core.h>
#include <ngx_http.h>

extern ngx_str_t* calc_diff_data(ngx_http_request_t *r, u_char* src_file_cnt, ngx_uint_t src_len, u_char* dst_file_cnt, ngx_uint_t dst_len);
