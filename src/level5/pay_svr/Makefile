FRAMEWORK_SRC := ../../

include $(FRAMEWORK_SRC)/rules.mak

MOD_NAME      := pay_svr

HTTP_BASE := ../http_base/
HTTP_BASE_LIB := libhb

C_SRCS        := $(shell find ./ -name "*.c" )
C_OBJS        := $(patsubst %.c,%.o,$(C_SRCS))

OBJS            := $(C_OBJS)

LDFLAG          := -L$(HTTP_BASE) -lhb
CFLAG_OBJS      := -Wall -Werror -I. -I$(HTTP_BASE) -I$(HTTP_BASE)/crypto/ -g  -fPIC -Wno-error=unused-function  -Wno-error=format-truncation
CFLAG_OBJS      += -I$(FRAMEWORK_SRC) -I$(FRAMEWORK_SRC)/lib/ 
TARGET_SO       := $(MOD_NAME)$(DY_MOD_EXT)



.PHONY: all
all: $(TARGET_SO) 

$(TARGET_SO):$(HTTP_BASE_LIB) $(OBJS) 
	$(CC) $(OBJS) $(LDFLAG) -shared -o $@

$(HTTP_BASE_LIB):
	make -C $(HTTP_BASE) 
	rm -f $(TARGET_SO) 

$(C_OBJS):%.o:%.c
	$(CC) $(CFLAG_OBJS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET_SO) 

