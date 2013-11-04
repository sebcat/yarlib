/*
Copyright (c) 2013, Sebastian Cato
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met: 

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer. 
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <errno.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>
#include "port.h"

typedef struct {
    yar_port_t start, end;
    unsigned int offset;
} port_range_t;

struct portspec_t {
    size_t nranges, rangeix;
    port_range_t *ranges;
};

#define port_range_init(_range, _start, _end) \
        do { \
            (_range)->start = (_start); \
            (_range)->offset = 0; \
            (_range)->end = (_end); \
        } while(0); 


int yar_port_from_str(yar_port_t *port, const char *str)
{
    unsigned long tmp;
    char *cptr;

    assert(port != NULL);
    assert(str != NULL);

    if (str == NULL || *str == '\0') {
        return -1;
    }

    tmp = strtoul(str, &cptr, 10);
    if (*cptr != '\0') {
        return -1;
    }

    if ((tmp == 0 && errno == EINVAL) || tmp > 65535) {
        return -1;
    }

    *port = (yar_port_t)tmp;
    return 0;
}

void yar_port_to_str(const yar_port_t port, char *dst, size_t dstlen)
{
    assert(dst != NULL);
    assert(dstlen > 0);

    snprintf(dst, dstlen, "%u", port);
    dst[dstlen-1] = '\0';
}

static int port_range_init_from_str(port_range_t *range, const char *rangestr)
{
    char buf[256], *cptr;
    yar_port_t startport, endport;

    strncpy(buf, rangestr, sizeof(buf));
    buf[sizeof(buf)-1] = '\0';
    cptr = strchr(buf, '-');
    if (cptr != NULL) {
        *cptr++ = '\0';
        if (yar_port_from_str(&startport, buf) != 0 ||
                yar_port_from_str(&endport, cptr) != 0) {
            return -1;
        }

    } else {
        if (yar_port_from_str(&startport, buf) != 0 || 
                yar_port_from_str(&endport, buf) != 0) {
            return -1;
        }
    }

    port_range_init(range, startport, endport);
    return 0;
}

static bool port_range_next(port_range_t *range, yar_port_t *dst)
{
    yar_port_t curr;
    assert(range != NULL);
    assert(dst != NULL);

    if (range->start > range->end) {
        curr = range->start - range->offset;
        if (curr < range->end) {
            return false;
        }
    } else if (range->start < range->end) {
        curr = range->start + range->offset;
        if (curr > range->end) {
            return false;
        }
    } else if (range->start == range->end) {
        curr = range->start;
        if (range->offset != 0) {
            return false;
        } 
    } else {
        assert(0);
        return false;
    }

    *dst = curr; 
    range->offset++;
    return true;
}

struct portspec_t *yar_portspec_new(const char *specstr)
{
    char *specbuf, *curr, *tok;
    struct portspec_t *spec;
    port_range_t *rangeptr;

    assert(specstr != NULL);

    specbuf = strdup(specstr);
    if (specbuf == NULL) {
        return NULL;
    }

    spec = malloc(sizeof(struct portspec_t));
    if (spec == NULL) {
        free(specbuf);
        return NULL;
    }

    memset(spec, 0, sizeof(struct portspec_t));
    curr = specbuf;
    while ((tok = strsep(&curr, " ,\t\r\n")) != NULL) {
        if (*tok == '\0') {
            continue;
        }
        
        if (spec->ranges == NULL) {
            spec->ranges = malloc(sizeof(port_range_t));
            if (spec->ranges == NULL) {
                yar_portspec_free(spec);
                free(specbuf);
                return NULL;
            }
        } else {
            rangeptr = realloc(spec->ranges, 
                    sizeof(port_range_t) * (spec->nranges+1));
            if (rangeptr == NULL) {
                yar_portspec_free(spec);
                free(specbuf);
                return NULL;
            }

            spec->ranges = rangeptr;
        }

        if (port_range_init_from_str(&spec->ranges[spec->nranges], tok)
                != 0) {
            free(specbuf);
            yar_portspec_free(spec);
            return NULL;
        }

        spec->nranges++;
    }

    free(specbuf);
    if (spec->nranges == 0) {
        yar_portspec_free(spec);
        spec = NULL;
    }

    return spec;
}

bool yar_portspec_next(struct portspec_t *spec, yar_port_t *port)
{
    assert(spec != NULL);
    assert(spec->ranges != NULL);
    if (!port_range_next(&spec->ranges[spec->rangeix], port)) {
        spec->rangeix++;
        if (spec->rangeix >= spec->nranges 
                || !port_range_next(&spec->ranges[spec->rangeix], port)) {
            spec->rangeix = spec->nranges;
            return false;
        }
    }

    return true;
}

void yar_portspec_reset(struct portspec_t *spec) 
{
    size_t i;
    assert(spec != NULL);
    assert(spec->ranges != NULL);

    spec->rangeix = 0;
    for(i=0; i<spec->nranges; i++) {
        spec->ranges[i].offset = 0;
    }
}

void yar_portspec_free(struct portspec_t *spec)
{
    if (spec != NULL) {
        if (spec->ranges != NULL) {
            free(spec->ranges);
        }

        free(spec);
    }
}

bool yar_portspec_is_expired(struct portspec_t *spec)
{
    return (spec->rangeix < spec->nranges) ? false : true;
}
