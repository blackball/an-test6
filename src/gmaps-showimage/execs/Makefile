all: showimagetile

BASEDIR := ../../tweak
COMMON := ../../tweak/an-common
include $(COMMON)/makefile.common
include $(COMMON)/makefile.qfits


CFLAGS  := -g -Wall
CFLAGS += $(QFITS_INC)
LDFLAGS := -lm

showimagetile: showimagetile.o $(QFITS_LIB)
