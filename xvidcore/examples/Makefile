#############################################################################
#
# XviD examples Makefile
#
# $Id: Makefile,v 1.3.2.4 2003-09-28 13:21:59 edgomez Exp $
#
#############################################################################

include ../build/generic/platform.inc

HDIR    = -I../src
CFLAGS  = -O2 $(ARCHITECTURE) $(BUS) $(ENDIANNESS) $(FEATURES) $(SPECIFIC_CFLAGS)
LDFLAGS = -lc -lm -lxvidcore

SOURCES= xvid_encraw.c xvid_decraw.c xvid_bench.c
OBJECTS=$(SOURCES:.c=.o)
TESTS=$(SOURCES:.c=)

all: $(TESTS)

xvid_encraw: xvid_encraw.o
	$(CC) $(LDFLAGS) -o $@ $<

xvid_encraw.o: xvid_encraw.c
	$(CC) $(CFLAGS) $(HDIR) -c $<

xvid_decraw: xvid_decraw.o
	$(CC) $(LDFLAGS) -o $@ $<

xvid_decraw.o: xvid_decraw.c
	$(CC) $(CFLAGS) $(HDIR) -c $<

xvid_bench: xvid_bench.o
	$(CC) $(LDFLAGS) -o $@ $<

xvid_bench.o: xvid_bench.c
	$(CC) $(CFLAGS) $(HDIR) -c $<

clean:
	rm -f $(OBJECTS) $(TESTS)
