BASEDIR := .
COMMON := an-common

.PHONY: all
all:

include $(COMMON)/makefile.common
include $(COMMON)/makefile.utils
include $(COMMON)/makefile.cfitsio
include $(COMMON)/makefile.qfits
include $(COMMON)/makefile.levmar
include $(COMMON)/makefile.libwcs
include $(COMMON)/makefile.libkd
include $(COMMON)/makefile.libtweak

LDFLAGS := $(LDFLAGS_DEF)
LDFLAGS += -lm

CFLAGS := $(CFLAGS_DEF)

CFLAGS += $(CFITS_INC)
CFLAGS += $(QFITS_INC)
CFLAGS += $(LEVMAR_INC)
CFLAGS += $(LIBWCS_INC)
CFLAGS += $(LIBKD_INC)
CFLAGS += $(LIBTWEAK_INC)
CFLAGS += $(ANUTILS_INC)
CFLAGS += -I.

TWEAK_OBJS := tweak.o # ezfits.o sip.o sip_util.o tweak_internal.o dgelsd.o
TEST_SIP_OBJS := test_sip.o # sip.o sip_util.o

all: $(REMAKE_LIBTWEAK) tweak test_sip

# tweak.c depends on qfits.h existing, which is installed along with the library in the
# "staging" directory qfits-an/stage.
tweak.c: $(QFITS_LIB)

tweak: $(TWEAK_OBJS) \
	$(LIBTWEAK_LIB) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB) $(CFITS_LIB) $(LEVMAR_LIB) \
	$(LIBWCS_LIB) $(LIBFORTRAN)

test_sip: $(TEST_SIP_OBJS) \
	$(LIBTWEAK_LIB) $(ANUTILS_LIB) $(CFITS_LIB) $(LIBWCS_LIB)

clean :
	rm tweak test_sip $(TWEAK_OBJS) $(TEST_SIP_OBJS)
