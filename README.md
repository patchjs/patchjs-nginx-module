# Patch.js module for Nginx

## Introduction

This module is used to calculate the difference between two files.

URL: http://static.domain.com/path/to/1.0.1/file-1.0.0.js

Result: {"m":[IS_MODIFIED],"l":[CHUNK_SIZE],"c":[CODE_DATA]}


## Configuration example

    location /static/css/ {
        patchjs on;
        patchjs_max_file_size 20;
    }
        
    location /static/js/ {
        patchjs on;
        patchjs_max_file_size 30;
    }


## Module directives

**patchjs** `on` | `off`

**default:** `patchjs off`

**context:** `http, server, location`

It enables the patchjs in a given context.

<br/>
<br/>

**patchjs\_max\_file\_size** `number`kb

**default:** `patchjs_max_file_size 1024`

**context:** `http, server, location`

Defines the **maximum** number of filesize that can be computed in a
given context. 


## Installation

./configure --with-debug --prefix=/usr/local/nginx --add-module=/path/to/patchjs-nginx-module

make

make install