
INTERNALS := 	kdtree_internal_dd.o \
	kdtree_internal_ff.o \
	kdtree_internal_du32.o \
	kdtree_internal_du16.o \
	kdtree_internal_u32u32.o \
	kdtree_internal_u16u16.o

KD := kdtree.o kdtree_dim.o $(INTERNALS)

CFLAGS := -g -O0 -Wall
LDFLAGS := -lm -g -O0 -Wall

all: tst

tst: tst.o $(KD)

$(INTERNALS): kdtree_internal.c

.PHONY: clean
clean:
	-rm $(KD) tst tst.o
