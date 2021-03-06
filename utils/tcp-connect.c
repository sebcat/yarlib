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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yarlib/yar.h>

#define NCURRCONNS      50
#define TICKRATE        10
#define IO_TIMEOUT_US   5000000

static void on_established(struct yar_endpoint *ep)
{
    char addr[ADDR_STRLEN];
    char port[16];
    
    yar_port_to_str(ep->port, port, sizeof(port));
    yar_addr_to_str(&ep->addr, addr);
    printf("open %s %s\n", addr, port);
    yar_endpoint_terminate(ep);
}

int main(int argc, char *argv[])
{
    struct yar_client cli;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <addrspec> <portspec>\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    memset(&cli, 0, sizeof(cli));
    cli.proto = ADDRPROTO_TCP;
    cli.on_established = on_established;
    cli.tr = TICKRATE;
    cli.ncc = NCURRCONNS;
    cli.to = IO_TIMEOUT_US;

    if (yar_connect(&cli, argv[1], argv[2]) != 0) {
        fprintf(stderr, "connection initiation failed\n");
        return EXIT_FAILURE;
    }
    
    yar_main();
    return EXIT_SUCCESS;
}

