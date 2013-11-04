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

/**
 * example usage:
 *     ./expand-addrdef 192.168.0.1-192.168.0.211,127.0.0.1/28 21-23,25
 *     ./expand-addrdef 'ff02::1-ff02::2,       10.2.1.2-10.2.1.6'
 */
#include <stdio.h>
#include <stdlib.h>
#include <yarlib/yar.h>

int print_addrs(const char *addrdef, const char *portspec)
{
    yar_addr_t addr;
    yar_port_t port;
    char addrstr[ADDR_STRLEN];
    yar_addrspec_t *aspec;
    yar_portspec_t *pspec;

    if ((aspec = yar_addrspec_new(addrdef)) == NULL) {
        return -1;
    }

    if (portspec != NULL) {
        pspec = yar_portspec_new(portspec);
        if (pspec == NULL) {
            yar_addrspec_free(aspec);
            return -1;
        }
    } else {
        pspec = NULL;
    }

    while (yar_addrspec_next(aspec, &addr)) {
        yar_addr_to_str(&addr, addrstr);
        if (pspec != NULL) {
            while (yar_portspec_next(pspec, &port)) {
                printf("%s %u\n", addrstr, port);
            }

            yar_portspec_reset(pspec);
        } else {
            printf("%s\n", addrstr);
        }
    }

    yar_addrspec_free(aspec);
    yar_portspec_free(pspec);

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: %s <addrspec> [portspec]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (print_addrs(argv[1], argv[2]) < 0) {
        fprintf(stderr, "error: unable to parse address/port definition\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
