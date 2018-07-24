CC=gcc
CFLAGS+=-g -Wall -Werror -std=gnu99 -O2 -I../vme
LDFLAGS+=`curl-config --libs`
LDFLAGS+=-lcunit

TARGETS=vmetest
OBJS= cunit_register.o test_aggregate.o test_delete.o test_execute.o test_insert.o \
	test_patch.o test_publish.o test_query.o test_random_stuff.o test_select.o \
	test_update.o test_utils.o cunit_main.o

all: $(TARGETS)

clean:
	$(RM) $(TARGETS)
	$(RM) $(OBJS)
	$(RM) CUnitAutomated-Listing.xml CUnitAutomated-Results.xml

vmetest: $(OBJS)
	$(CC) -o $@ $^ ../vme/libvme.a $(LDFLAGS)

.PHONY: all clean
