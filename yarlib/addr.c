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

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include "addr.h"

struct yar_addr_iterator_t {
    struct yar_addr_t curr, end;
    bool expired;
};

struct yar_addrspec_t {
    char *strmem;
    size_t nspecs, specpos;
    char **specs;
    struct yar_addr_iterator_t iter;
};



#define ASTEP_UPWARD         1
#define ASTEP_DOWNWARD      -1

int yar_addr_init(struct yar_addr_t *addr, const char *addrstr)
{
    struct addrinfo hints, *addrs = NULL, *curr = NULL;
    int ret = -1;

    assert(addr != NULL);
    assert(addrstr != NULL);

    /* inet_pton is probably faster than getaddrinfo, however it does
      not deal with IPv6 zone-id */
    
    if (addr == NULL || addrstr == NULL) {
        return -1;
    }

    memset(addr, 0, sizeof(*addr));
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (getaddrinfo(addrstr, NULL, &hints, &addrs) == 0) {
        for(curr = addrs; curr != NULL; curr = curr->ai_next) {
            if (curr->ai_family == AF_INET || curr->ai_family == AF_INET6) {
                addr->af = curr->ai_family;
                addr->saddr_len = curr->ai_addrlen;
                memcpy(&addr->saddr, curr->ai_addr, curr->ai_addrlen);
                ret = 0;
                break;
            }
        }
    }
    
    if (addrs != NULL) {
        freeaddrinfo(addrs);
    }

    return ret;
}

/* NB: The length of dst buf must be ADDR_STRLEN or more */
void yar_addr_to_str(const struct yar_addr_t *addr, char *dst)
{
    assert(addr != NULL);
    assert(dst != NULL);

    memset(dst, 0, ADDR_STRLEN);
    getnameinfo((struct sockaddr *)&addr->saddr, addr->saddr_len, 
            dst, ADDR_STRLEN, NULL, 0, NI_NUMERICHOST);
}

void yar_addr_to_addrport_str(const struct yar_addr_t *addr, 
        unsigned short port, char *dst, size_t len)
{   
    char addrbuf[ADDR_STRLEN];
    
    assert(addr != NULL);
    assert(dst != NULL);

    yar_addr_to_str(addr, addrbuf);
    if (addr->af == AF_INET) {
        snprintf(dst, len, "%s:%hu", addrbuf, port);
    } else {
        snprintf(dst, len, "[%s]:%hu", addrbuf, port);
    }

    dst[len-1] = '\0';
}

bool yar_addr_cmp(const struct yar_addr_t *t1, const struct yar_addr_t *t2, 
        int *cmpval)
{
    bool retval;

    /* cmpval OK to be NULL (test for comparability) */
    assert(t1 != NULL);
    assert(t2 != NULL);
    
    retval = false;

    if (t1->af == t2->af) {
        if (t1->af == AF_INET) {
            if (cmpval != NULL) {
                const struct sockaddr_in *s1 = 
                        (struct sockaddr_in *)&t1->saddr;
                const struct sockaddr_in *s2 = 
                        (struct sockaddr_in *)&t2->saddr;
                *cmpval = memcmp(&s1->sin_addr, &s2->sin_addr, 
                        sizeof(struct in_addr)); 
            }

            retval = true;
        } else if (t1->af == AF_INET6) {
                const struct sockaddr_in6 *s1 = 
                        (struct sockaddr_in6 *)&t1->saddr;
                const struct sockaddr_in6 *s2 = 
                        (struct sockaddr_in6 *)&t2->saddr;

                if (s1->sin6_scope_id == s2->sin6_scope_id) {
                    if (cmpval != NULL) { 
                        *cmpval = memcmp(&s1->sin6_addr, &s2->sin6_addr, 
                                sizeof(struct in6_addr)); 
                    }
                 
                    retval = true;
                }
        }
    }

    return retval;
}

void yar_addr_copy(struct yar_addr_t *dst, const struct yar_addr_t *src)
{
    assert(dst != NULL);
    assert(src != NULL);
    memcpy(dst, src, sizeof(struct yar_addr_t));
}

void yar_addr_copy_to_storage(const struct yar_addr_t *addr, 
        unsigned short port, struct sockaddr_storage *saddr, socklen_t *len)
{
    assert(addr != NULL);
    assert(addr->af == AF_INET || addr->af == AF_INET6);

    memcpy(saddr, &addr->saddr, addr->saddr_len);
    if (addr->af == AF_INET) {
        ((struct sockaddr_in *)saddr)->sin_port = htons(port);
    } else if (addr->af == AF_INET6) {
        ((struct sockaddr_in6 *)saddr)->sin6_port = htons(port);
    } 

    if (len != NULL) {
        *len = addr->saddr_len;
    }
}

static void addr_clear_mask_bits(struct yar_addr_t *addr, unsigned long mask)
{
    struct in6_addr *addr6 = NULL;
    struct in_addr *addr4 = NULL;
    int i;
    unsigned long bits;

    assert(addr != NULL);
 
    if (addr->af == AF_INET) {
        addr4 = &((struct sockaddr_in*)&addr->saddr)->sin_addr;
        bits = mask > 32 ? 0 : 32-mask;
        addr4->s_addr &= htonl(0xffffffff << bits);
    } else if (addr->af == AF_INET6) {
        addr6 = &((struct sockaddr_in6*)&addr->saddr)->sin6_addr;
        i=15;
        bits = mask > 128 ? 0 : 128-mask;
        do {
            if (bits > 8) {
                addr6->s6_addr[i] = 0;
                bits -= 8;
            } else {
                addr6->s6_addr[i] &= (0xff << bits);
                bits = 0;
            }
        } while (i-- > 0 && bits > 0);
    }
}

static void addr_set_mask_bits(struct yar_addr_t *addr, unsigned long mask) 
{
    struct in6_addr *addr6 = NULL;
    struct in_addr *addr4 = NULL;
    int i;
    unsigned long bits;

    assert(addr != NULL);

    if (addr->af == AF_INET) {
        addr4 = &((struct sockaddr_in*)&addr->saddr)->sin_addr;
        bits = mask > 32 ? 0 : 32-mask;
        addr4->s_addr |= ~htonl(0xFFFFFFFF << bits);
    } else if (addr->af == AF_INET6) {
        addr6 = &((struct sockaddr_in6*)&addr->saddr)->sin6_addr;
        i=15;
        bits = mask > 128 ? 0 : 128-mask;
        do {
            if (bits > 8) {
                addr6->s6_addr[i] = 0xff;
                bits -= 8;
            } else {
                addr6->s6_addr[i] |= (0xff >> (8-bits));
                bits = 0;
            }
        } while (i-- > 0 && bits > 0);
    }
}

static void addr_step(struct yar_addr_t *addr, int dir)
{
    struct in_addr *saddr;
    struct in6_addr *saddr6;
    uint32_t *piece;

    assert(addr != NULL);
    assert(dir == ASTEP_UPWARD || dir == ASTEP_DOWNWARD);

    if (addr->af == AF_INET) {
        saddr = &((struct sockaddr_in*)&addr->saddr)->sin_addr;
        if (dir == ASTEP_UPWARD) {
            saddr->s_addr = htonl(ntohl(saddr->s_addr)+1);
        } else {
            saddr->s_addr = htonl(ntohl(saddr->s_addr)-1);
        }
    } else if (addr->af == AF_INET6) {
        saddr6 = &((struct sockaddr_in6*)&addr->saddr)->sin6_addr;
        piece =  (uint32_t*)&saddr6->s6_addr[12];
        if (dir == ASTEP_UPWARD) {
            do {
                *piece = htonl(ntohl(*piece)+1);
            } while(*piece == 0 && --piece >= (uint32_t*)&saddr6->s6_addr);
        } else {
            do {
                *piece = htonl(ntohl(*piece)-1);
            } while(*piece == 0xFFFFFFFF && 
                    --piece >= (uint32_t*)&saddr6->s6_addr);
        }
    
    }
}

static int addr_step_towards(struct yar_addr_t *addr, 
        const struct yar_addr_t *end)
{
    int cmp = -1, retval = -1;

    assert(addr != NULL);
    assert(end != NULL);

    if (yar_addr_cmp(addr, end, &cmp)) {
        if (cmp == 0) {
            retval = 0;
        } else {
            if (cmp < 0) {
                addr_step(addr, ASTEP_UPWARD);
            } else if (cmp > 0) {
                addr_step(addr, ASTEP_DOWNWARD);
            }

            retval = 1;
        }
    }

    return retval;
}

static int addr_iterator_init_from_str(struct yar_addr_iterator_t *iter, 
        const char *str)
{
    /*
      string formats:
        addr
        addr-addr
        addr/netmask
    */
    char *cptr = NULL, *endptr = NULL;
    unsigned long mask;
    char buf[256];

    assert(iter != NULL);
    assert(str != NULL);

    iter->expired = false;

    strncpy(buf, str, sizeof(buf));
    buf[sizeof(buf)-1] = '\0';

    if ((cptr = strchr(buf, '/')) != NULL) {
        /* netmask */
        *cptr = '\0';
        cptr++;
        if (*cptr == '\0') {
            return -1;
        }

        mask = strtoul(cptr, &endptr, 10);
        if (mask > 128 || *endptr != '\0') {
            return -1;    
        }
        
        if (yar_addr_init(&iter->curr, buf) == -1) {
            return -1;
        }

        yar_addr_copy(&iter->end, &iter->curr);
        addr_clear_mask_bits(&iter->curr, mask);
        addr_set_mask_bits(&iter->end, mask);
    } else if ((cptr = strchr(buf, '-')) != NULL) {
        /* dash range */
        *cptr = '\0';
        cptr++;
        if (*cptr == '\0') {
            return -1; 
        }

        if (yar_addr_init(&iter->curr, buf) == -1) {
            return -1;
        }

        if (yar_addr_init(&iter->end, cptr) == -1) {
            return -1;
        }

        if (yar_addr_cmp(&iter->curr, &iter->end, NULL) == false) {
            return -1;
        }
    } else {
        if (yar_addr_init(&iter->curr, buf) == -1) {
            return -1;
        }

        yar_addr_copy(&iter->end, &iter->curr);
    }

    return 0;
}

static bool addr_iterator_next(struct yar_addr_iterator_t *iter, 
        struct yar_addr_t *addr) 
{
    int ret;

    assert(iter != NULL);
    assert(addr != NULL);

    if (iter->expired) {
        return false;
    }

    yar_addr_copy(addr, &iter->curr);
    ret = addr_step_towards(&iter->curr, &iter->end);
    /* ret == -1 would be bad, as it would indicate that the addresses in
       the iterator are not comparable, something we checked for
       at initiation */
    assert(ret != -1);
    if (ret == 0) {
        iter->expired = true;
    }

    return true;
}

struct yar_addrspec_t *yar_addrspec_new(const char *specstr)
{
    struct yar_addr_iterator_t iter;
    struct yar_addrspec_t *spec;
    char *curr, *tok, **tmp;

    assert(specstr != NULL);

    spec = malloc(sizeof(struct yar_addrspec_t));
    if (spec == NULL) {
        return NULL;
    }

    memset(spec, 0, sizeof(struct yar_addrspec_t));
    spec->strmem = strdup(specstr);
    if (!spec->strmem) {
        free(spec);
        return NULL;
    }

    curr = spec->strmem;
    spec->specs = NULL;
    spec->nspecs = 0;
    spec->specpos = 0;
    while ((tok = strsep(&curr, ", \t\r\n")) != NULL) {
        if (*tok == '\0') {
            continue;
        }

        /* validate by parsing, but don't save the parsed
           iterator (time/memory trade-off) */
        if (addr_iterator_init_from_str(&iter, tok) != 0) {
            yar_addrspec_free(spec);
            return NULL;
        }

        if (spec->specs != NULL) {
            tmp = realloc(spec->specs, sizeof(char*)*(spec->nspecs+1));
            if (tmp == NULL) {
                yar_addrspec_free(spec);
                return NULL;
            }

            spec->specs = tmp;

            spec->specs[spec->nspecs] = tok;
        } else {
            spec->specs = malloc(sizeof(char*));
            spec->specs[0] = tok;
        }
        
        spec->nspecs++;
    }

    if (spec->nspecs > 0) {
        addr_iterator_init_from_str(&spec->iter, spec->specs[spec->specpos]);
    } else {
        yar_addrspec_free(spec);
        spec = NULL;
    }

    return spec;
}

void yar_addrspec_free(struct yar_addrspec_t *spec)
{
    if (spec != NULL) {
        if (spec->strmem != NULL) {
            free(spec->strmem);
        }

        if (spec->specs != NULL) {
            free(spec->specs);
        }

        free(spec);
    }
}

bool yar_addrspec_next(struct yar_addrspec_t *spec, struct yar_addr_t *outaddr) 
{
    assert(spec != NULL);
    assert(outaddr != NULL);

    if (addr_iterator_next(&spec->iter, outaddr) == false) {
        spec->specpos++;
        if (spec->specpos < spec->nspecs 
                && addr_iterator_init_from_str(&spec->iter, 
                        spec->specs[spec->specpos]) == 0
                && addr_iterator_next(&spec->iter, outaddr)) {

            return true; 
        } else {
            /* expired, specpos is set to nspecs to signal this */
            spec->specpos = spec->nspecs;
            return false;
        }
    }

    return true;
}

bool yar_addrspec_is_expired(struct yar_addrspec_t *spec)
{
    return (spec->specpos < spec->nspecs) ? false : true;
}

