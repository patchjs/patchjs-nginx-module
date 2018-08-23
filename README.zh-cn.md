# Patch.js Nginx Module

## 介绍

通过 Nginx Module 实时计算两个不同版本文件之间的 Diff 文件。

## URL 规范

[STATIC_URL_PREFIX]/[VERSION]/[FILE_NAME]-[LOCAL_VERSION].[FILE_EXT]

例如: http://static.domain.com/path/to/1.0.1/file-1.0.0.js

### DIFF 结果样例

{"m":[IS_MODIFIED],"l":[CHUNK_SIZE],"c":[CODE_DATA]}

例如: {"m":true,"l":20,"c":['var num = 0;']}


## 安装

./configure --prefix=/usr/local/nginx --add-module=/path/to/patchjs-nginx-module

make

make install


## Configuration 样例

location /static/css/ {
    patchjs on;
    patchjs_max_file_size 1024;
}
    
location /static/js/ {
    patchjs on;
    patchjs_max_file_size 1024;
}


## Module 指令

**patchjs** `on` | `off`

**default:** `patchjs off`

**context:** `http, server, location`

在指定的上下文环境，开启 patchjs 的功能。

<br/>

**patchjs\_max\_file\_size** `number`kb

**default:** `patchjs_max_file_size 1024`

**context:** `http, server, location`

指定允许最大文件的大小。
