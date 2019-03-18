
FRAMEWORK_SRC := ../../

include $(FRAMEWORK_SRC)/rules.mak

MOD_NAME      := libhb

C_SRCS        := $(shell find ./ -name "*.c" )
C_OBJS        := $(patsubst %.c,%.o,$(C_SRCS))

OBJS            := $(C_OBJS)

LDFLAG          :=  
CFLAG_OBJS      := -Wall -Werror -I. -I./crypto  -g  -fPIC -Wno-error=unused-function -Wno-error=format-truncation -DTEST_CASES=1
CFLAG_OBJS      += -I$(FRAMEWORK_SRC) -I$(FRAMEWORK_SRC)/lib/ 
TARGET_LIB      := $(MOD_NAME).a


.PHONY: all
all: $(TARGET_LIB) 

$(TARGET_LIB):$(OBJS)
	$(AR) -r $@ $(OBJS) 

$(C_OBJS):%.o:%.c
	$(CC) $(CFLAG_OBJS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET_LIB) 

