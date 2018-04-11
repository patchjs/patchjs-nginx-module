#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    ngx_flag_t   enable;
    ngx_uint_t   max_file_size;
} ngx_http_patchjs_loc_conf_t;


static ngx_int_t ngx_http_patchjs_open_file(ngx_http_core_loc_conf_t *ccf, ngx_http_request_t *r, ngx_open_file_info_t *of, ngx_str_t *filename);
static ngx_int_t ngx_http_patchjs_read_file(u_char *buffer, ngx_http_request_t *r, ngx_open_file_info_t *of);

static ngx_int_t ngx_http_patchjs_init(ngx_conf_t *cf);
static void *ngx_http_patchjs_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_patchjs_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);


/* static ngx_str_t  ngx_http_patchjs_default_types[] = {
    ngx_string("application/x-javascript"),
    ngx_string("text/css"),
    ngx_null_string
}; */


static ngx_command_t  ngx_http_patchjs_commands[] = {
    { ngx_string("patchjs"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_patchjs_loc_conf_t, enable),
      NULL },
    { ngx_string("patchjs_max_file_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_patchjs_loc_conf_t, max_file_size),
      NULL },
      ngx_null_command
};


static ngx_http_module_t  ngx_http_patchjs_module_ctx = {
    NULL,                                /* preconfiguration */
    ngx_http_patchjs_init,                /* postconfiguration */

    NULL,                                /* create main configuration */
    NULL,                                /* init main configuration */

    NULL,                                /* create server configuration */
    NULL,                                /* merge server configuration */

    ngx_http_patchjs_create_loc_conf,     /* create location configuration */
    ngx_http_patchjs_merge_loc_conf       /* merge location configuration */
};


ngx_module_t  ngx_http_patchjs_module = {
    NGX_MODULE_V1,
    &ngx_http_patchjs_module_ctx,         /* module context */
    ngx_http_patchjs_commands,            /* module directives */
    NGX_HTTP_MODULE,                     /* module type */
    NULL,                                /* init master */
    NULL,                                /* init module */
    NULL,                                /* init process */
    NULL,                                /* init thread */
    NULL,                                /* exit thread */
    NULL,                                /* exit process */
    NULL,                                /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t ngx_http_patchjs_handler(ngx_http_request_t *r) {
    size_t                      root;
    ngx_str_t                   path;
    ngx_int_t                   rc;
    ngx_buf_t                  *b;
    ngx_chain_t                 out;
    u_char                     *last;
    ngx_open_file_info_t        of;
    ngx_http_core_loc_conf_t   *ccf;
    ngx_http_patchjs_loc_conf_t *clcf;

    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_DECLINED;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_patchjs_module);

    if (!clcf->enable) {
        return NGX_DECLINED;
    }

    rc = ngx_http_discard_request_body(r);

    if (rc != NGX_OK) {
        return rc;
    }

    last = ngx_http_map_uri_to_path(r, &path, &root, 0);
    // 
    if (last == NULL) {
       return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    path.len = last - path.data;

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "patchjs path: \"%s\"", path.data);
    
    ngx_int_t local_version_start = 0, local_version_end = 0, version_start = 0, version_end = 0, dot_cnt = 0, slash_cnt = 0;
    
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "path_data: \"%s\"", path.data);
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "path_len: \"%d\"", path.len);

    ngx_int_t len = path.len, i;
    u_char* r_path = path.data;
    u_char* p = r_path + len - 1;

    for(i = len - 1; i >= 0; i --) {
        if (*p == '.' && dot_cnt == 0) {
            dot_cnt ++;
            local_version_end = i - 1;
        } else if (*p == '-') {
            local_version_start = i + 1;
        } else if (*p == '/') {
            if (slash_cnt == 0) {
                version_end = i - 1;
            } else if (slash_cnt == 1) {
                version_start = i + 1;
                break;
            }
            slash_cnt++;
        }
        p--;
    }

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "version_start: \"%d\"", version_start);
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "version_end: \"%d\"", version_end);
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "local_version_start: \"%d\"", local_version_start);
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "local_version_end: \"%d\"", local_version_end);

    if (local_version_start == 0 || local_version_end == 0 || version_start == 0 || version_end == 0 || dot_cnt == 0 || slash_cnt == 0) {
        return NGX_DECLINED;
    }

    size_t version_path_len = len - local_version_end + local_version_start - 2; 
    u_char *version_path_data = ngx_pcalloc(r->pool, version_path_len + 1);

    size_t local_version_len = local_version_end - local_version_start + 1;
    u_char *local_version_data = ngx_palloc(r->pool, local_version_len);

    // reset pointer
    p = r_path;
    ngx_int_t k = 0, h = 0;
    for (i = 0; i < len; i ++) {
        if (i < local_version_start - 1 || i > local_version_end ) {
            *(version_path_data + k) = *p;
            k ++;
        } else if (i >= local_version_start && i <= local_version_end) {
            *(local_version_data + h) = *p;
            h ++;
        }
        p ++;
    }

    *(version_path_data + k) = '\0';// hack

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "version_path_data: \"%s\"", version_path_data);
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "version_path_len: \"%d\"", version_path_len);

    ngx_str_t version_path, local_version, local_version_path;
    version_path.len = version_path_len;
    version_path.data = version_path_data;

    local_version.len = local_version_len;
    local_version.data = local_version_data;
   
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "version_path: \"%V\"", &version_path);
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "local_version: \"%V\"", &local_version);

    p = version_path_data;
    size_t local_version_path_len = len - version_end + version_start - 2;
    u_char *local_version_path_data = ngx_palloc(r->pool, local_version_path_len + 1);
    ngx_int_t m, n, g = 0;

    for (m = 0; m < len; m ++) {
        if (m < version_start || m > version_end) {
            *(local_version_path_data + g) = *p;
            g ++;
        }
        if (m == version_start) {
            for(n = 0; n < (ngx_int_t)local_version_len; n ++){
                *(local_version_path_data + g) = *(local_version_data + n);
                g ++;
            }
        }
        p ++;
    }
    local_version_path_data[g] = '\0';// hack

    local_version_path.len = local_version_path_len;
    local_version_path.data = local_version_path_data;
   
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "local_version_path: \"%V\"", &local_version_path);
    
    ccf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    ngx_memzero(&of, sizeof(ngx_open_file_info_t));

    of.read_ahead = ccf->read_ahead;
    of.directio = ccf->directio;
    of.valid = ccf->open_file_cache_valid;
    of.min_uses = ccf->open_file_cache_min_uses;
    of.errors = ccf->open_file_cache_errors;
    of.events = ccf->open_file_cache_events;


    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "version_path_data: \"%s\"", version_path.data);
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "version_path_len: \"%d\"", version_path_len);

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "local_version_path_data: \"%s\"", local_version_path.data);
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "local_version_path_len: \"%d\"", local_version_path_len);

    ngx_str_t filename = version_path;//ngx_string("/Users/heli/Desktop/static/css/1.0.0/a.css");

    ngx_int_t open_file_ret = ngx_http_patchjs_open_file(ccf, r, &of, &filename);

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "is_file: \"%d\"", of.is_file);

    if (open_file_ret != NGX_OK) {
        return open_file_ret;
    }

    u_char *version_buffer = ngx_pcalloc(r->pool, of.size);

    if (version_buffer == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_int_t read_file_ret = ngx_http_patchjs_read_file(version_buffer, r, &of);
    
    if (read_file_ret != NGX_OK) {
        return read_file_ret;
    }

    ngx_str_t type = ngx_string("text/plain");

    size_t res_len = of.size;

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = res_len;
    r->headers_out.content_type = type;

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    b = ngx_create_temp_buf(r->pool, res_len);

    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_memcpy(b->pos, version_buffer, res_len);
    b->last = b->pos  + res_len;
    b->last_buf = 1;

    out.buf = b;
    out.next = NULL;

    return ngx_http_output_filter(r, &out);
}

static ngx_int_t ngx_http_patchjs_read_file(u_char *buffer, ngx_http_request_t *r, ngx_open_file_info_t *of) {

    ngx_int_t ret = ngx_read_fd(of->fd, buffer, of->size);

    if (ret == NGX_ERROR) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    return NGX_OK;
}

static ngx_int_t ngx_http_patchjs_open_file(ngx_http_core_loc_conf_t *ccf, ngx_http_request_t *r, ngx_open_file_info_t *of, ngx_str_t *filename) {
    ngx_int_t rc;
    if (ngx_open_cached_file(ccf->open_file_cache, filename, of, r->pool) != NGX_OK) {
        switch (of->err) {
            case 0:
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            case NGX_ENOENT:
            case NGX_ENOTDIR:
            case NGX_ENAMETOOLONG:
                rc = NGX_HTTP_NOT_FOUND;
                break;
            case NGX_EACCES:
                rc = NGX_HTTP_FORBIDDEN;
                break;
            default:
                rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
                break;
        }
        if (rc != NGX_HTTP_NOT_FOUND || ccf->log_not_found) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, of->err,
                          "%s \"%V\" failed", of->failed, filename);
        }
        return rc;
    }

    if (!of->is_file) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, ngx_errno,
                      "\"%V\" is not a regular file", filename);
        return NGX_HTTP_NOT_FOUND;
    }

    return NGX_OK;
}

static void *ngx_http_patchjs_create_loc_conf(ngx_conf_t *cf) {
    ngx_http_patchjs_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_patchjs_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->enable = NGX_CONF_UNSET;
    conf->max_file_size = NGX_CONF_UNSET_UINT;

    return conf;
}


static char *ngx_http_patchjs_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child) {
    ngx_http_patchjs_loc_conf_t *prev = parent;
    ngx_http_patchjs_loc_conf_t *conf = child;

    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_uint_value(conf->max_file_size, prev->max_file_size, 10);

    return NGX_CONF_OK;
}


static ngx_int_t ngx_http_patchjs_init(ngx_conf_t *cf) {
    ngx_http_handler_pt       *h;
    ngx_http_core_main_conf_t *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_patchjs_handler;

    return NGX_OK;
}
