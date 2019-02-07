
FRAMEWORK_SRC := ../../

include $(FRAMEWORK_SRC)/rules.mak


JAVAC            := javac

MODULE_NAME      := my_jvm

JAMVM_BASE    := /home/user1/work/jamvm-improved/
#JAMVM_BASE    := /home/user1/work/jamvm-2.0.0/
JAMVM_SRC     := $(JAMVM_BASE)/src/
JAMVM_LIB     := $(JAMVM_BASE)/install/lib/

C_SRCS        := $(shell find ./ -name "*.c" )
C_OBJS        := $(patsubst %.c,%.o,$(C_SRCS))

OBJS            := $(C_OBJS)

LDFLAG          :=  
CFLAG_OBJS      := -Wall -Werror -I. -g  -fPIC 
CFLAG_OBJS      += -I$(JAMVM_SRC) -I$(FRAMEWORK_SRC) -I$(FRAMEWORK_SRC)/lib/ 


# TODO: your own ldflags here
EXTRA_LDFLAG    := -L$(JAMVM_LIB) -ljvm 
LDFLAG          += $(EXTRA_LDFLAG)

# TODO: your own cflags here
EXTRA_CFLAG     := 
CFLAG_OBJS      += $(EXTRA_CFLAG)

WEBAPPDIR       := webapp/


TARGET_SO       := $(MODULE_NAME)$(DY_MOD_EXT)


.PHONY: all
all: $(TARGET_SO)  mkwebapp

mkwebapp:
	$(JAVAC) $(WEBAPPDIR)/*.java -cp $(WEBAPPDIR):/home/user1/work/java_libs/servlet-api.jar

$(TARGET_SO):$(OBJS)
	$(CC) $(OBJS) $(LDFLAG) -shared -o $@

$(C_OBJS):%.o:%.c
	$(CC) $(CFLAG_OBJS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET_SO) $(WEBAPPDIR)/*.class

