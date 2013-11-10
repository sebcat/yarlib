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

#ifndef __YAR_H
#define __YAR_H

#include "port.h"
#include "addr.h"

/* read validator return values */
#define RVALIDATOR_INCORRECT        -1 /* terminate the connection */
#define RVALIDATOR_INCOMPLETE       0  /* wait for more data */
#define RVALIDATOR_OK               1  /* pass the data to the handler */
typedef int (*yar_read_validator)(const void *data, size_t len);

typedef enum {
    ADDRPROTO_TCP,
    ADDRPROTO_UDP
} yar_addrproto_t;

typedef struct yar_endpoint_handle yar_endpoint_handle_t;

typedef void (*yar_endpoint_data_free_cb)(void *data);

struct yar_endpoint {
    yar_endpoint_handle_t *handle;
    yar_addr_t addr;
    yar_port_t port;
};

typedef void (*yar_endpoint_handler)(struct yar_endpoint *ep);

struct yar_client {
    yar_addrproto_t proto;

    /* behavioral settings */
    unsigned int tr;    /* tick rate (ticks / second) */
    unsigned int cpt;   /* connect(2) calls per tick */
    unsigned int ncc;   /* number of concurrent connections */
    unsigned int to;    /* I/O timeout in microseconds */

    /* event callbacks */
    yar_endpoint_handler on_established;
    yar_endpoint_handler on_read;
    yar_endpoint_handler on_eof;
    yar_endpoint_handler on_timeout;
    yar_endpoint_handler on_error;

    /* read buffer message validator */
    yar_read_validator read_validator;
};


void yar_endpoint_set_cdata(struct yar_endpoint_handle *eph, void *cdata,
        yae_endpoint_data_free_cb free_cb);
void *yar_endpoint_get_cdata(struct yar_endpoint_handle *eph);
void *yar_endpoint_read(yar_endpoint_handle_t *eph, size_t *len);
void yar_endpoint_write(yar_endpoint_handle_t *eph, const void *data,
        size_t len);
const char *yar_endpoint_get_errmsg(yar_endpoint_handle_t *eph);
void yar_endpoint_terminate(struct yar_endpoint *ep);

int yar_connect(struct yar_client *cli, const char *addrspec, 
        const char *portspec);

int yar_main();

#endif
