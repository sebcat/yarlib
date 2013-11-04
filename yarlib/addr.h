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
#ifndef __ADDR_H
#define __ADDR_H

#include <stdbool.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>

#define ADDR_STRLEN 128

typedef struct yar_addr_t {
    int af;
    struct sockaddr_storage saddr;
    socklen_t saddr_len;
} yar_addr_t; 

typedef struct yar_addrspec_t yar_addrspec_t;


/**
 * yar_addr_init --
 *     Initialize a addr representation from a string representing the
 *     address of said addr
 *
 * @return -1 on error, 0 on success
 */
int yar_addr_init(yar_addr_t *addr, const char *addrstr);

/**
 * yar_addr_to_str --
 *     Writes the string representation of a addr to the buffer designated
 *     by dst. dst must point to a buffer at least ADDR_STRLEN bytes in
 *     size
 *
 */
void yar_addr_to_str(const yar_addr_t *addr, char *dst);
void yar_addr_to_addrport_str(const struct yar_addr_t *addr, 
        unsigned short port, char *dst, size_t len);


/**
 * yar_addr_cmp --
 *     Compare two addrs ip IP addresses and zone-id, if applicable
 *
 *     The result of a successful comparison is stored in cmpval. This
 *     value is less than, equal to or greater than zero, depending on
 *     if t1 is less than, equal to or greater than t2
 *     
       if cmpval is NULL, only test for comparability
 *
 * returns false if the addresses are incomparable, true otherwise
 */
bool yar_addr_cmp(const yar_addr_t *t1, const yar_addr_t *t2, int *cmpval);

/**
 * yar_addr_copy --
 *     Copy the addr information contained in src to dst
 */
void yar_addr_copy(yar_addr_t *dst, const yar_addr_t *src);

/**
 * yar_addr_copy_to_storage --
 *     copy the sockaddr structure to saddr
 */
void yar_addr_copy_to_storage(const struct yar_addr_t *addr, 
        unsigned short port, struct sockaddr_storage *saddr, socklen_t *len);
/**
 * addrspec_new --
 *     Allocate and initiate an address specification, which is a sequence of 
 *     one or more address ranges in string representation separated by 
 *     spaces and/or commas
 */
yar_addrspec_t *yar_addrspec_new(const char *specstr);

/**
 * yar_addrspec_free --
 *     deallocate memory allocated by addrspec_new
 */
void yar_addrspec_free(yar_addrspec_t *spec);

/**
 * yar_addrspec_next --
 *     fill in the next address. Returns true if the next address was fetched,
 *     false if the spec is expired
 */
bool yar_addrspec_next(yar_addrspec_t *spec, yar_addr_t *outaddr); 

/**
 * yar_addrspec_is_expired --
 *     returns true if spec is expired, false if not
 */
bool yar_addrspec_is_expired(struct yar_addrspec_t *spec);
#endif
