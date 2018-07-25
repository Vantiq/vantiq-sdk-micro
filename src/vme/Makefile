CC=gcc
CFLAGS+=-g -Wall -Werror -std=gnu99 -O2 -fPIC
LDFLAGS+=`curl-config --libs`

TARGETS=libvme.a libvme.so
OBJS=buf.o cjson.o config.o log.o utils.o vantiq_client.o vme.o
all: $(TARGETS)

clean:
	$(RM) $(TARGETS)
	$(RM) $(OBJS)

libvme.a: $(OBJS)
	$(AR) rcs $@ $^

libvme.so: $(OBJS)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

.PHONY: all clean
