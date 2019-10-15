FRAMEWORK_SRC := ../../

include $(FRAMEWORK_SRC)/rules.mak

MOD_NAME      := hsyncd

HTTP_BASE      := ../http_base/
THRIFT_BASE    := /home/user1/work/thrift-0.12.0/install
C_GLIB_DIR     := ./gen-c_glib/


C_SRCS        := $(shell find ./ -name "*.c" )
C_OBJS        := $(patsubst %.c,%.o,$(C_SRCS))

OBJS            := $(C_OBJS)

LDFLAG          := -L$(THRIFT_BASE)/lib -lthrift -lthrift_c_glib -lglib-2.0 -lgobject-2.0 #-L$(HTTP_BASE) -lhb
CFLAG_OBJS      := -Wall -Werror -I. -I$(HTTP_BASE) -g  -fPIC -Wno-error=unused-function  -Wno-error=format-truncation
CFLAG_OBJS      += -I$(FRAMEWORK_SRC) -I$(FRAMEWORK_SRC)/lib/ 
CFLAG_OBJS      += -Wall -Werror -g -I. -I$(C_GLIB_DIR) -I$(THRIFT_BASE)/include -I/usr/include/glib-2.0/
TARGET_SO       := $(MOD_NAME)$(DY_MOD_EXT)


.PHONY: all
all: $(TARGET_SO) 

$(TARGET_SO):$(OBJS)
	$(CC) $(OBJS) $(LDFLAG) -shared -o $@

$(C_OBJS):%.o:%.c
	$(CC) $(CFLAG_OBJS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET_SO) 

