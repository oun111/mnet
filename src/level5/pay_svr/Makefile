FRAMEWORK_SRC := ../../

include $(FRAMEWORK_SRC)/rules.mak

MOD_NAME      := pay_svr

HTTP_BASE := ../http_base/
HTTP_BASE_LIB := libhb

REDIS_BASE    := /home/user1/work/redis-4.0.10/install/
REDIS_LIB     := $(REDIS_BASE)/lib/
REDIS_INCLUDE := $(REDIS_BASE)/include/

C_SRCS        := $(shell find ./ -name "*.c" )
C_OBJS        := $(patsubst %.c,%.o,$(C_SRCS))

OBJS            := $(C_OBJS)

LDFLAG          := -L$(HTTP_BASE) -lhb -L$(REDIS_LIB) -lhiredis
CFLAG_OBJS      := -Wall -Werror -I.  -g  -fPIC -Wno-error=unused-function  -Wno-error=format-truncation -DTEST_CASES=1
CFLAG_OBJS      += -I$(FRAMEWORK_SRC) -I$(FRAMEWORK_SRC)/lib/ -I$(HTTP_BASE) -I$(HTTP_BASE)/crypto/ -I$(REDIS_INCLUDE)
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

