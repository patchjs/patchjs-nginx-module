#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "ngx_diff.h"


typedef struct {
    ngx_flag_t   enable;
    ngx_uint_t   max_file_size;
} ngx_http_patchjs_loc_conf_t;


static ngx_int_t ngx_http_patchjs_open_file(ngx_http_core_loc_conf_t *ccf, ngx_http_request_t *r, ngx_open_file_info_t *of, ngx_str_t *filename);
static ngx_int_t ngx_http_patchjs_read_file(u_char *buffer, ngx_http_request_t *r, ngx_open_file_info_t *of);

static ngx_int_t ngx_http_patchjs_init(ngx_conf_t *cf);
static void *ngx_http_patchjs_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_patchjs_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static ngx_int_t ngx_http_patchjs_handler(ngx_http_request_t *r);


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
    ngx_conf_merge_uint_value(conf->max_file_size, prev->max_file_size, 1024);

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

ngx_int_t ngx_http_patchjs_get_file_buffer(ngx_http_request_t *r, ngx_http_core_loc_conf_t *ccf, ngx_str_t *root_path, ngx_str_t *base_filename, ngx_str_t *ext, ngx_str_t *version, ngx_str_t *buffer)
{
    ngx_open_file_info_t of;
    ngx_memzero(&of, sizeof(ngx_open_file_info_t));
    of.read_ahead = ccf->read_ahead;
    of.directio = ccf->directio;
    of.valid = ccf->open_file_cache_valid;
    of.min_uses = ccf->open_file_cache_min_uses;
    of.errors = ccf->open_file_cache_errors;
    of.events = ccf->open_file_cache_events;

    ngx_str_t filename;
    filename.len = root_path->len + 1 + version->len + 1 + base_filename->len + 1 + ext->len;
    filename.data = ngx_palloc(r->pool, sizeof(u_char) * filename.len);

    u_char *p = filename.data;
    ngx_memcpy(p, root_path->data, root_path->len);
    p += root_path->len;
    *p++ = '/';

    ngx_memcpy(p, version->data, version->len);
    p += version->len;
    *p++ = '/';

    ngx_memcpy(p, base_filename->data, base_filename->len);
    p += base_filename->len;
    *p++ = '.';

    ngx_memcpy(p, ext->data, ext->len);
    p += ext->len;

    ngx_int_t open_file_ret = ngx_http_patchjs_open_file(ccf, r, &of, &filename);
    if (open_file_ret != NGX_OK) {
        return open_file_ret;
    }

    buffer->data = ngx_pcalloc(r->pool, of.size);
    if (buffer == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    buffer->len = of.size;

    ngx_int_t read_file_ret = ngx_http_patchjs_read_file(buffer->data, r, &of);
    if (read_file_ret != NGX_OK) {
        return read_file_ret;
    }

    return NGX_OK;
}

static ngx_int_t ngx_http_patchjs_handler(ngx_http_request_t *r) 
{
    ngx_str_t root_path;                                    /* nginx root directory */
    ngx_str_t base_filename, ext;                           /* filename and ext */
    ngx_str_t pre_path;                                     /* prefix path of request url */

    ngx_str_t version, local_version;
    ngx_uint_t dot_cnt = 0, slash_cnt = 0, across_cnt = 0, count = 0;

    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_DECLINED;
    }

    /* get location config */
    ngx_http_patchjs_loc_conf_t *clcf = ngx_http_get_module_loc_conf(r, ngx_http_patchjs_module);
    if (!clcf->enable) {
        return NGX_DECLINED;
    }

    /* discard body */
    ngx_int_t rc = ngx_http_discard_request_body(r);
    if (rc != NGX_OK) {
        return rc;
    }

    /* save resource path  */
    size_t root;
    ngx_str_t path;
    u_char *last = ngx_http_map_uri_to_path(r, &path, &root, 0);
    if (last == NULL) {
       return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    path.len = last - path.data;

    // url standard
    u_char *p = path.data + path.len - 1;
    for (ngx_int_t i = path.len - 1; i >= 0; i--) {
        if (*p == '.' && dot_cnt == 0) {
            dot_cnt++;
            ext.len = count;
            ext.data = p+1;
        } else if (*p == '-' && across_cnt == 0) {
            across_cnt++;
            local_version.data = p + 1;
            local_version.len = ext.data - local_version.data - 1;
        } else if (*p == '/') {
            if (slash_cnt == 0) {
                base_filename.data = p + 1;
                base_filename.len = local_version.data - base_filename.data - 1;
            } else if (slash_cnt == 1) {
                version.data = p + 1;
                version.len = base_filename.data - version.data - 1;
                break;
            }
            slash_cnt++;
        }

        count++;
        p--;
    }

    // basic check url format
    if (version.len != local_version.len || ngx_strncmp(version.data, local_version.data, version.len) <= 0) {
        return NGX_DECLINED;
    }

    pre_path.len = p - path.data;
    pre_path.data = path.data;

    ngx_http_core_loc_conf_t *ccf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    root_path.data = ccf->root.data;
    root_path.len = ccf->root.len;

    /* get file content */
    ngx_str_t version_buffer, local_version_buffer;
    ngx_int_t ret = ngx_http_patchjs_get_file_buffer(r, ccf, &pre_path, &base_filename, &ext, &local_version, &local_version_buffer);
    if (ret != NGX_OK) {
        return NGX_ERROR;
    }

    ret = ngx_http_patchjs_get_file_buffer(r, ccf, &pre_path, &base_filename, &ext, &version, &version_buffer);
    if (ret != NGX_OK) return NGX_ERROR;

    /* diff */
    ngx_str_t *res = calc_diff_data(r, version_buffer.data, version_buffer.len, local_version_buffer.data, local_version_buffer.len);

    ngx_str_t type = ngx_string("text/plain");// todo
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = res->len;
    r->headers_out.content_type = type;

    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    ngx_buf_t *b = ngx_create_temp_buf(r->pool, res->len); //todo
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_memcpy(b->pos, res->data, res->len);  
    b->last = b->pos + res->len;
    b->last_buf = 1;

    ngx_chain_t out;
    out.buf = b;
    out.next = NULL;

    return ngx_http_output_filter(r, &out);
}
