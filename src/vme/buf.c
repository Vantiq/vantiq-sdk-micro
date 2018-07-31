// buf.c
//
// a dynamically expanding buffer
//
//  Copyright © 2018 VANTIQ. All rights reserved.


#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "vme.h"

#define MIN_BUF_SIZE 64
#define LRG_BUF_SIZE 1024*1024
#define SML_BUF_SIZE 1024

vmebuf_t *vmebuf_ensure_incr_size(vmebuf_t *buf, size_t incrsize)
{
    vmebuf_t *result = vmebuf_ensure_size(buf, (buf == NULL ? 0 : buf->len) + incrsize);
    assert(result->len <= result->limit);
    return result;
}

vmebuf_t *vmebuf_truncate(vmebuf_t *buf)
{
    assert(buf != NULL);
    buf->len = 0;
    return buf;
}

vmebuf_t *vmebuf_ensure_size(vmebuf_t *buf, size_t newSize)
{
    if (buf == NULL)
    {
        // allocate a new buffer
        buf = malloc(sizeof(vmebuf_t));
        // todo: malloc returns NULL
        buf->limit = 0;
        buf->data = NULL;
        buf->len = 0;
    }
    newSize = (newSize < MIN_BUF_SIZE ? MIN_BUF_SIZE : newSize);

    if (buf->limit < newSize) {
        buf->data = realloc(buf->data, newSize);
        assert(buf->data != NULL);
        buf->limit = newSize;
    } else if (newSize <= SML_BUF_SIZE && buf->limit >= LRG_BUF_SIZE) {
        // shrink the buffer in case one outlying use is chewing up lots of mem
        buf->data = realloc(buf->data, newSize);
        buf->limit = newSize;
    }
    
    return buf;
}

void vmebuf_push(vmebuf_t *buf, char c)
{
    assert(buf != NULL);
    vmebuf_ensure_size(buf, buf->len+MIN_BUF_SIZE);

    buf->data[buf->len++] = c;
    assert(buf->len <= buf->limit);
}

void vmebuf_concat(vmebuf_t *dst, const char *src, size_t len)
{
    assert(dst != NULL);
    assert(src != NULL);

    vmebuf_ensure_size(dst, dst->len+len+len/2);

    for (size_t i = 0; i < len; i++)
        dst->data[dst->len++] = src[i];
    assert(dst->len <= dst->limit);
}

char * vmebuf_tostr(vmebuf_t *buf)
{
    assert(buf != NULL);

    char *str = malloc(buf->len + 1);
    assert(str != NULL);

    memcpy(str, buf->data, buf->len);
    str[buf->len] = '\0';

    return str;
}

void vmebuf_dealloc(vmebuf_t *buf)
{
    assert(buf != NULL);
    free(buf->data);
    buf->data = NULL;
    free(buf);
}
