all: showimagetile

BASEDIR := ../../tweak
COMMON := ../../tweak/an-common
include $(COMMON)/makefile.common
include $(COMMON)/makefile.qfits
include $(COMMON)/makefile.libtweak
include $(COMMON)/makefile.anfiles
include $(COMMON)/makefile.utils


CFLAGS  := -g -Wall
CFLAGS += $(QFITS_INC)
CFLAGS += $(LIBTWEAK_INC)
CFLAGS += $(ANUTILS_INC)
CFLAGS += $(ANFILES_INC)
LDFLAGS := -lm

showimagetile: showimagetile.o $(QFITS_LIB) $(LIBTWEAK_LIB) $(ANUTILS_LIB) $(ANFILES_LIB)
