include rules.mak

FRAMEWORK := mnet
GEN_DIR:= $(shell pwd)/.gen/

C_SRCS         := $(notdir $(shell find . -maxdepth 1 -name "*.c"))
C_OBJS         := $(patsubst %.c,$(GEN_DIR)/%.o,$(C_SRCS))
#C_ST_MOD_SRCS  := $(notdir $(shell find ./level5/example_http -name "*.c"))
#C_OBJS         += $(patsubst %.c,$(GEN_DIR)/%.o,$(C_ST_MOD_SRCS))

OBJS            := $(C_OBJS)


#
# dynamic modules
#
MODULE_SRC_DIR  := level5
#DY_MODS_SRCS        := $(shell find $(MODULE_SRC_DIR) -maxdepth 1 -type d ! -name "$(MODULE_SRC_DIR)")
DY_MODS_SRCS        := $(MODULE_SRC_DIR)/http_base $(MODULE_SRC_DIR)/pay_svr #$(MODULE_SRC_DIR)/crypto_svr #$(MODULE_SRC_DIR)/mysql #$(MODULE_SRC_DIR)/example_http #$(MODULE_SRC_DIR)/my_jvm


DY_MODS_INSTALL_DIR := ../modules/


#
# static modules
#
ST_MOD_CFLAGS   := 
ST_MOD_LDFLAGS  := 


MOD_VER         := $(shell grep -i "__VER_STR__" ./$(FRAMEWORK).h|awk '{print $$3}'|sed 's/\"//g')
LDFLAG          := -Wall -g -pthread -lrt -L./lib/ -lkp  -ldl -lcrypto -lssl -T customize.lds -rdynamic $(ST_MOD_LDFLAGS) #.$(MOD_VER) 
CFLAG_OBJS      := -Wall -Werror -g -I. -I./lib/ -I./$(MODULE_SRC_DIR) -I./$(MODULE_SRC_DIR)/mysql/ -DTEST_CASES=1  -DDY_MOD_EXT="\"$(DY_MOD_EXT)\""  $(ST_MOD_CFLAGS) -DFRAMEWORKNAME="\"$(FRAMEWORK)\""
TARGET_EXEC     := $(FRAMEWORK)_test
 

.PHONY: all
all: mkdir  $(TARGET_EXEC) dy_mods_install

mkdir:
	$(shell mkdir  $(GEN_DIR))

$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@  $(LDFLAG)

dy_mods_install: 
	for i in $(DY_MODS_SRCS); \
    do \
    cd $$i; \
    make all; \
    cd ../../ ; \
    cp $$i/*$(DY_MOD_EXT) $(DY_MODS_INSTALL_DIR)/ ; \
    done ; 

dy_mods_clean: 
	for i in $(DY_MODS_SRCS); \
    do \
    cd $$i; \
    make clean; \
    cd ../../ ; \
    done ; \
	rm -f $(DY_MODS_INSTALL_DIR)/*

%.o:../%.c
	$(CC) $(CFLAG_OBJS) -c $< -o $@

#%.o:../level5/%.c
#	$(CC) $(CFLAG_OBJS) -c $< -o $@

#%.o:../level5/example_http/%.c
#	$(CC) $(CFLAG_OBJS) -c $< -o $@

#%.o:../level5/my_jvm/%.c
#	$(CC) $(CFLAG_OBJS) -c $< -o $@

.PHONY: clean
clean: dy_mods_clean
	rm -rf $(OBJS) $(TARGET_EXEC) $(GEN_DIR)

.PHONY: distclean
distclean:clean
	rm -rf cscope.* 
	rm -rf tags

