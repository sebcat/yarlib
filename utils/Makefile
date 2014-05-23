
include ../global.mk

CFLAGS+=-I.. -L../yarlib
LIBS+=-levent -lyarlib
TARGETS=http-head expand-addrdef tcp-connect

all: $(TARGETS) 

http-head: http-head.c 
	$(CC) $(CFLAGS) -o $@ $? $(LIBS)

expand-addrdef: expand-addrdef.c
	$(CC) $(CFLAGS) -o $@ $? $(LIBS)

tcp-connect: tcp-connect.c
	$(CC) $(CFLAGS) -o $@ $? $(LIBS)

clean:
	$(RM) $(TARGETS)
