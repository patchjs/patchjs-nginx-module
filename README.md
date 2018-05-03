# Patch.js module for Nginx

## Introduction

This module is used to calculate the difference between two files.

## URL Specification

[STATIC_URL_PREFIX]/[VERSION]/[FILE_NAME]-[LOCAL_VERSION].[FILE_EXT]

eg: http://static.domain.com/path/to/1.0.1/file-1.0.0.js

### DIFF Result

{"m":[IS_MODIFIED],"l":[CHUNK_SIZE],"c":[CODE_DATA]}

eg: {"m":true,"l":20,"c":['var num = 0;']}


## Installation

./configure --prefix=/usr/local/nginx --add-module=/path/to/patchjs-nginx-module

make

make install


## Configuration example

location /static/css/ {
    patchjs on;
    patchjs_max_file_size 1024;
}
    
location /static/js/ {
    patchjs on;
    patchjs_max_file_size 1024;
}


## Module directives

**patchjs** `on` | `off`

**default:** `patchjs off`

**context:** `http, server, location`

It enables the patchjs in a given context.

<br/>

**patchjs\_max\_file\_size** `number`kb

**default:** `patchjs_max_file_size 1024`

**context:** `http, server, location`

Defines the **maximum** number of filesize that can be computed in a
given context. 
