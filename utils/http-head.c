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
    static const char req[] = "HEAD / HTTP/1.1\r\nHost: %s\r\n\r\n";
    char sendbuf[512], hoststr[64];

    yar_addr_to_addrport_str(&ep->addr, (unsigned short)ep->port, hoststr,
            sizeof(hoststr));
    snprintf(sendbuf, sizeof(sendbuf), req, hoststr);
    sendbuf[sizeof(sendbuf)-1] = '\0';
    yar_endpoint_write(ep->handle, sendbuf, strlen(sendbuf));  
}

static void on_read(struct yar_endpoint *ep)
{
    size_t len;
    char *read, addrbuf[ADDR_STRLEN], portbuf[16];

    read = yar_endpoint_read(ep->handle, &len);
    if (read != NULL) {
        yar_addr_to_str(&ep->addr, addrbuf);
        yar_port_to_str(ep->port, portbuf, sizeof(portbuf));
        printf("%s %s\n%.*s\n\n\n", addrbuf, portbuf, (int)len, read);
    }

}

static int read_validator(const void *data, size_t len)
{
    return strnstr(data, "\r\n\r\n", len) == NULL ? RVALIDATOR_INCOMPLETE :
            RVALIDATOR_OK;
}

int main(int argc, char *argv[])
{
    struct yar_client cli;
    int retval = EXIT_FAILURE;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <addrspec> <portspec>\n", argv[0]);
        return retval; 
    }

    memset(&cli, 0, sizeof(cli));
    cli.on_established = on_established;
    cli.on_read = on_read;
    cli.read_validator = read_validator;
    cli.ncc = NCURRCONNS;
    cli.tr = TICKRATE;
    cli.to = IO_TIMEOUT_US;

    if (yar_connect(&cli, argv[1], argv[2]) != 0) {
        fprintf(stderr, "yar_connect failed\n");
    } else {
        yar_main();
        retval = EXIT_SUCCESS;
    }
    
    return retval; 
}

