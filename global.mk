CC=clang
CFLAGS=
CFLAGS+=-Werror -Wall -pedantic -ansi -g 
CFLAGS+=-I/usr/local/include

RM=rm -f
AR=ar -rcs
LIBS=-L/usr/local/lib/event2 -levent-2.0
LDFLAGS=
