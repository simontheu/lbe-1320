CFLAGS=-g
LBPROGRAMS=lbe-1320

HIDLIB=-L. -lhidapi-hidraw -Wl,-rpath,.

all: lbe-1320-utils

libhidapi-hidraw.so:
	ln -s libhidapi-hidraw.so.0 libhidapi-hidraw.so

lbe-1320-utils: lbe-1320.c
	gcc ${CFLAGS} -o lbe-1320 lbe-1320.c -I.

all-clean:
	rm ${LBPROGRAMS} libhidapi-hidraw.so

clean:
	rm ${PROGRAMS}


