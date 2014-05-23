include ../global.mk

.PHONY=all clean
all: libyarlib.a

libyarlib.a: addr.c port.c yar.c
	$(CC) $(CFLAGS) -c addr.c
	$(CC) $(CFLAGS) -c port.c
	$(CC) $(CFLAGS) -c yar.c
	$(AR) libyarlib.a *.o

clean:
	$(RM) libyarlib.a *.o
