BASEDIR := ..
COMMON := .
include $(COMMON)/makefile.common
include $(COMMON)/makefile.utils
include $(COMMON)/makefile.qfits

CFLAGS := $(CFLAGS_DEF)
CFLAGS += -O0
CFLAGS += $(QFITS_INC)

LIBUTILS := libutils.a

all: $(LIBUTILS) $(UTILS_OBJS)

$(LIBUTILS): $(UTILS_OBJS)
	$(AR) rc $@ $^
