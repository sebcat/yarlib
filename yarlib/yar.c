
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
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <assert.h>

#include "yar.h"

/**
 * all events share a single event_base by design. This is important, because
 * of the intended use case for this library. This also means multi-threading
 * is a bad idea.
 */
static struct event_base *_evbase = NULL;

#define YARINIT() \
    do { \
        if (_evbase == NULL) { \
            _evbase = event_base_new(); \
            assert(_evbase != NULL); \
        } \
    } while (0);

struct yar_ticker {
    struct event *ev;
    yar_ticker_func f;
    void *data;
    yar_cleanup_func free_cb;
};

#define CONNECT_TICKER_FLG_FINISHED_DISPATCHING    1  
struct yar_connect_ticker {
    struct yar_client *cli;
    yar_addrspec_t *addrspec;
    yar_portspec_t *portspec;
    yar_addr_t curr_addr;
    struct event *ev;
    unsigned int ncurrent; /* number of established connections */
    unsigned int flags;
};

struct yar_endpoint_handle {
    struct yar_connect_ticker *ticker;
    struct bufferevent *bev;
    
    /* caller data, for storing stuff related to an endpoint connection */
    void *cdata;
    yar_cleanup_func free_cb;
};

static struct yar_endpoint_handle *yar_endpoint_handle_new(
        struct yar_connect_ticker *ticker,
        struct bufferevent *bev)
{
    struct yar_endpoint_handle *eph;

    assert(ticker != NULL);
    assert(bev != NULL);

    eph = malloc(sizeof(*eph));
    if (eph == NULL) {
        return NULL;
    }

    memset(eph, 0, sizeof(*eph));
    eph->ticker = ticker;
    eph->bev = bev;
    return eph;
}

static void yar_endpoint_handle_free(struct yar_endpoint_handle **eph)
{
    assert(eph != NULL);

    if (*eph != NULL) {
        if ((*eph)->bev != NULL) {
            bufferevent_free((*eph)->bev);
        }
    
        if ((*eph)->ticker != NULL) {
            (*eph)->ticker->ncurrent--;
        }

        if ((*eph)->free_cb != NULL && (*eph)->cdata != NULL) {
            (*eph)->free_cb((*eph)->cdata);
        }

        free(*eph);
        *eph = NULL;
    }
}


static struct yar_connect_ticker *yar_connect_ticker_new(
        struct yar_client *cli, 
        const char *addrspec,
        const char *portspec)
{
    struct yar_connect_ticker *ticker;

    ticker = malloc(sizeof(*ticker));
    if (ticker == NULL) {
        return NULL;
    }

    ticker->cli = cli;
    ticker->ncurrent = 0;
    ticker->flags = 0;
    ticker->ev = NULL;
    ticker->addrspec = yar_addrspec_new(addrspec);
    if (ticker->addrspec == NULL) {
        free(ticker);
        return NULL;
    }

    if (!yar_addrspec_next(ticker->addrspec, &ticker->curr_addr)) {
        yar_addrspec_free(ticker->addrspec);
        free(ticker);
        return NULL;
    }

    ticker->portspec = yar_portspec_new(portspec);
    if (ticker->portspec == NULL) {
        yar_addrspec_free(ticker->addrspec);
        free(ticker);
        return NULL;
    }

    return ticker;
}

static void yar_connect_ticker_free(void *data)
{
    struct yar_connect_ticker *ticker = data;
    if (ticker != NULL) {
        if (ticker->addrspec != NULL) {
            yar_addrspec_free(ticker->addrspec);
        }

        if (ticker->portspec != NULL) {
            yar_portspec_free(ticker->portspec);
        }

        if (ticker->ev != NULL) {
            event_free(ticker->ev);
        }

        free(ticker);
    }
}
static void yar_client_on_read(struct bufferevent *bev, void *ctx)
{
    struct yar_endpoint *ep = ctx;
    struct yar_client *cli;
    struct evbuffer *evb;
    size_t len;
    unsigned char *data = NULL;
    int ret;

    assert(ep != NULL);
    assert(ep->handle != NULL);
    
    if (ep->handle->bev == NULL) {
        ep->handle->bev = bev;
    }
    
    cli = ep->handle->ticker->cli;
    assert(cli != NULL);

    evb = bufferevent_get_input(bev);
    len = evbuffer_get_length(evb);
    if (len > 0) {
        if (cli->read_validator != NULL) {
            data = evbuffer_pullup(evb, len);
            ret = cli->read_validator(data, len);
            if (ret == RVALIDATOR_INCORRECT) {
                yar_endpoint_handle_free(&ep->handle);
                free(ep);
            } else if (ret == RVALIDATOR_INCOMPLETE) {
                return;
            }
        } 

        cli->on_read(ep);
        evbuffer_drain(evb, len);
        if (ep->handle == NULL) {
            free(ep);
        }
    }
}

static void yar_client_on_event(struct bufferevent *bev, short events, 
        void *ctx)
{
    struct yar_endpoint *ep = ctx;
    struct yar_client *cli;
    assert(ep != NULL);
    assert(ep->handle != NULL);
    
    if (ep->handle->bev == NULL) {
        ep->handle->bev = bev;
    }

    cli = ep->handle->ticker->cli;
    assert(cli != NULL);
    
    if (events & (BEV_EVENT_ERROR|BEV_EVENT_EOF|BEV_EVENT_TIMEOUT)) {
        if (cli->on_error != NULL && events & BEV_EVENT_ERROR) {
            cli->on_error(ep);
        } else if (cli->on_eof != NULL && events & BEV_EVENT_EOF) {
            cli->on_eof(ep);
        } else if (cli->on_timeout != NULL && events & BEV_EVENT_TIMEOUT) {
            cli->on_timeout(ep);
        }
        
        if (ep->handle != NULL) {
            yar_endpoint_handle_free(&ep->handle);
        }

        free(ep);
    } else if (events & BEV_EVENT_CONNECTED) {
        if (cli->on_established != NULL) {
            cli->on_established(ep);

            if (ep->handle == NULL) {
                free(ep);
            }
        }
    }
}

static void yar_connect_ticker_dispatch_connections(
        struct yar_connect_ticker *ticker, unsigned int nconns)
{
    struct yar_endpoint *ep = NULL;
    struct yar_client *cli;
    yar_port_t port;
    struct bufferevent *bev;
    bufferevent_data_cb on_read;
    struct timeval tv;
    evutil_socket_t fd;
    struct sockaddr_storage ss;
    socklen_t sslen = 0;

    assert(ticker != NULL);
    cli = ticker->cli;
    assert(cli != NULL);
    assert(nconns > 0);

    while (nconns > 0) {
        if (!yar_portspec_next(ticker->portspec, &port)) {
            if (!yar_addrspec_next(ticker->addrspec, &ticker->curr_addr)) {
                ticker->flags |= CONNECT_TICKER_FLG_FINISHED_DISPATCHING;
                return;
            } else {
                yar_portspec_reset(ticker->portspec);
                continue;
            }
        }

        ep = malloc(sizeof(*ep));
        if (!ep) {
            break;
        }

        yar_addr_copy(&ep->addr, &ticker->curr_addr);
        ep->port = port;
        ep->handle = NULL;
        
        if (cli->proto == ADDRPROTO_UDP) {
            fd = socket(ticker->curr_addr.af, SOCK_DGRAM, 
                    IPPROTO_UDP);
        } else {
            fd = socket(ticker->curr_addr.af, SOCK_STREAM, 
                    IPPROTO_TCP);
        }

        evutil_make_socket_nonblocking(fd);
        bev = bufferevent_socket_new(_evbase, fd, 
                BEV_OPT_CLOSE_ON_FREE);
        if (cli->on_read != NULL) {
            on_read = yar_client_on_read;
        } else {
            on_read = NULL;
        }

        ep->handle = yar_endpoint_handle_new(ticker, bev);
        bufferevent_setcb(bev, on_read, NULL, yar_client_on_event, ep);
        if (cli->to > 0) {
            tv.tv_sec = cli->to  / 1000000;
            tv.tv_usec = cli->to % 1000000;
            bufferevent_set_timeouts(bev, &tv, &tv);
        }

        if (on_read == NULL) {
            bufferevent_enable(bev, EV_WRITE);
        } else {
            bufferevent_enable(bev, EV_WRITE|EV_READ);
        }

        nconns--;
        ticker->ncurrent++;
        yar_addr_copy_to_storage(&ticker->curr_addr, 
                (unsigned short)port, &ss, &sslen);
        if (bufferevent_socket_connect(bev, (struct sockaddr *)&ss, 
                    sslen) < 0) { 
            /* unable to initiate connection attempt
               error should be handled by yar_client_on_event */
            continue;
        }
    }
}

static int yar_connect_ticker_cb(void *data)
{
    struct yar_connect_ticker *ticker = data;
    struct yar_client *cli;
    unsigned int nconn_max = 0; 
    
    assert(ticker != NULL);
    assert(ticker->addrspec != NULL);
    assert(ticker->portspec != NULL);
    cli = ticker->cli;
    assert(cli != NULL);

    if (ticker->flags & CONNECT_TICKER_FLG_FINISHED_DISPATCHING) {
        if (ticker->ncurrent == 0) {
            return TICKER_DONE;
        }

        return TICKER_CONT;
    }

    /* determine maximum number of allowed connections for this tick */
    if (cli->tr == 0 || (cli->cpt == 0 && cli->ncc == 0)) {
        nconn_max = UINT_MAX;
    } else {
        if (cli->cpt > 0) {
            if (cli->ncc > 0) {
                assert(cli->ncc >= ticker->ncurrent);
                nconn_max = cli->ncc - ticker->ncurrent;
                if (nconn_max > cli->cpt) {
                    nconn_max = cli->cpt;
                }
            } else {
                nconn_max = cli->cpt;
            }
        } else {
            assert(cli->ncc > 0);
            assert(cli->ncc >= ticker->ncurrent);
            nconn_max = cli->ncc - ticker->ncurrent;
        }
    }

    if (ticker->ncurrent < nconn_max) {
        yar_connect_ticker_dispatch_connections(ticker, nconn_max);
    }

    return TICKER_CONT;
}

const char *yar_endpoint_get_errmsg(yar_endpoint_handle_t *eph)
{
    const char *cptr;

    if (eph == NULL) {
        /* if eph is null, we couln't establish the connection */
        return "connection failed";
    } else {
        /* Getting some valgrind errors on line below */
        cptr = evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR());
        return cptr != NULL ? cptr : "unknown error";
    }
}

void yar_endpoint_set_cdata(struct yar_endpoint_handle *eph, void *cdata,
        yar_cleanup_func free_cb)
{
    assert(eph != NULL);
    eph->cdata = cdata;
    eph->free_cb = free_cb;
}

void *yar_endpoint_get_cdata(struct yar_endpoint_handle *eph)
{
    assert(eph != NULL);
    return eph->cdata; 
}

void *yar_endpoint_read(yar_endpoint_handle_t *eph, size_t *len)
{
    size_t readlen;
    struct evbuffer *evb;
    void *ret;
    assert(eph != NULL);
    assert(len != NULL);
    assert(eph->bev != NULL);

    evb = bufferevent_get_input(eph->bev);
    readlen = evbuffer_get_length(evb);
    if (readlen > 0) {
        ret = evbuffer_pullup(evb, readlen);
        *len = readlen;
        return ret;
    } else {
        *len = 0;
        return NULL;
    }
}

void yar_endpoint_write(yar_endpoint_handle_t *eph, const void *data, 
        size_t len)
{   
    assert(eph != NULL);
    assert(eph->bev != NULL);
    assert(data != NULL);

    bufferevent_write(eph->bev, data, len);
}

void yar_endpoint_terminate(struct yar_endpoint *ep)
{
    yar_endpoint_handle_free(&ep->handle);
}

static void yar_ticker_cb(evutil_socket_t cfd, short what, void *data)
{
    struct yar_ticker *t;
    assert(data != NULL);
    
    t = data;
    if (t->f(t->data) == TICKER_DONE) {
        t->free_cb(t->data);
        event_free(t->ev);
        free(t);
    }
}

int yar_ticker(yar_ticker_func func, unsigned int tick_rate, void *data,
        yar_cleanup_func free_cb)
{
    struct yar_ticker *t;
    struct timeval tv;
    assert(func != NULL);
    assert(tick_rate > 0);

    YARINIT();

    t = malloc(sizeof(*t));
    if (!t) {
        return -1;
    }

    if (tick_rate > 1000000) {
        tick_rate = 1000000;
    }

    tv.tv_sec = 0; 
    tv.tv_usec = 1000000 / tick_rate;
    t->f = func;
    t->data = data;
    t->free_cb = free_cb;
    t->ev = event_new(_evbase, -1, EV_TIMEOUT|EV_PERSIST, 
            yar_ticker_cb, t);
    if (t->ev == NULL) {
        free(t);
        return -1;
    }
    
    event_add(t->ev, &tv);
    return 0;
}

int yar_connect(struct yar_client *cli, const char *addrspec, 
        const char *portspec)
{
    struct yar_connect_ticker *ticker;
    unsigned int tick_rate;

    assert(cli != NULL);
    assert(addrspec != NULL);
    assert(portspec != NULL);

    YARINIT();

    if (cli->proto != ADDRPROTO_TCP && cli->proto != ADDRPROTO_UDP) {
        return -1;
    }

    ticker = yar_connect_ticker_new(cli, addrspec, portspec);
    if (ticker == NULL) {
        return -1;
    }

    if (cli->tr == 0 || cli->tr > 1000000) {
        cli->tr = 0;
        tick_rate = 2;  
    } else {
        tick_rate = cli->tr;
    }

    if (yar_ticker(yar_connect_ticker_cb, tick_rate, ticker, 
            yar_connect_ticker_free) < 0) {
        yar_connect_ticker_free(ticker);
        return -1;
    }

    return 0;
}

int yar_main()
{
    int retval;

    YARINIT();
    retval = event_base_dispatch(_evbase);
    event_base_free(_evbase);
    _evbase = NULL;
    return retval;
}

