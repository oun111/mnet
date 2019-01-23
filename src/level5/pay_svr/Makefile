FRAMEWORK_SRC := ../../

include $(FRAMEWORK_SRC)/rules.mak

MOD_NAME      := pay_svr

C_SRCS        := $(shell find ./ -name "*.c" )
C_OBJS        := $(patsubst %.c,%.o,$(C_SRCS))

OBJS            := $(C_OBJS)

LDFLAG          :=  
CFLAG_OBJS      := -Wall -Werror -I. -g  -fPIC -Wno-error=unused-function -Wno-error=format-truncation -DTEST_CASES=0
CFLAG_OBJS      += -I$(FRAMEWORK_SRC) -I$(FRAMEWORK_SRC)/lib/ 
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

