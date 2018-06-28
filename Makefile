CFLAGS+=-g -Wall -Werror -std=gnu99 -O2
LDFLAGS+=`curl-config --libs`

TARGETS=vipo dpisim 
OBJS=buf.o dpi_client.o vantiq_client.o config.o

all: $(TARGETS) .gitignore

clean:
	$(RM) $(TARGETS)
	$(RM) $(OBJS) vipo.o dpisim.o

test: $(OBJS) test.o
	$(CC) -o $@ $^ $(LDFLAGS)

vipo: $(OBJS) vipo.o
	$(CC) -o $@ $^ $(LDFLAGS)

dpisim: dpisim.o
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: all clean
