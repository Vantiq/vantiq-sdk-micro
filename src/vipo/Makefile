CC=gcc
CFLAGS+=-g -Wall -Werror -std=gnu99 -O2 -I../vme
LDFLAGS+=`curl-config --libs`

TARGETS=vipo
OBJS=dpi_client.o log.o vipo.o

all: $(TARGETS)

clean:
	$(RM) $(TARGETS)
	$(RM) $(OBJS)

vipo: $(OBJS)
	$(CC) -o $@ $^ ../vme/libvme.a $(LDFLAGS)

.PHONY: all clean
