BASEDIR := ..
COMMON := .
include $(COMMON)/makefile.common
include $(COMMON)/makefile.utils
include $(COMMON)/makefile.qfits

CFLAGS := $(CFLAGS_DEF)
CFLAGS += -O0
CFLAGS += $(QFITS_INC)

all: $(ANUTILS_LIB)

$(ANUTILS_LIB): fitsioutils.o
	$(AR) rc $@ $^
