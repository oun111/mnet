#!/usr/bin/python3 -B

from sys import argv
import os
import re



class ModTemplateGenerator(object):

  module_name = None

  #########################
  #
  # c source template
  #
  ########################
  c_file_template = """
#include "connection.h"
#include "instance.h"
#include "module.h"
#include "log.h"
#include "MOD_NAME.h"


static int parse_cmd_line(int argc, char *argv[]);


int MOD_NAME_rx(Network_t net, connection_t pconn) 
{
  return 0;
}

int MOD_NAME_tx(Network_t net, connection_t pconn)
{
  return 0;
}

int MOD_NAME_pre_init(int argc, char *argv[])
{
  if (parse_cmd_line(argc,argv))
    return 1;

  return 0;
}

void MOD_NAME_release()
{
}

int MOD_NAME_init(Network_t net)
{
  log_info("done!\\n");

  return 0;
}


static 
struct module_struct_s g_module = {

  .name = "MOD_NAME",

  .id = -1,

  .dyn_handle = NULL,

  .opts[inbound_l5] = {
    .init     = MOD_NAME_init,
    .release  = MOD_NAME_release,
    .rx       = MOD_NAME_rx,
    .tx       = MOD_NAME_tx,
  },
};

static 
void MOD_NAME_dump_params()
{
  log_info("module '%s' param list: =================\\n",THIS_MODULE->name);
  log_info("module param list end =================\\n");
}

static
int parse_cmd_line(int argc, char *argv[])
{
  for (int i=1; i<argc; i++) {
    if (!strcmp(argv[i],"-h")) {
      printf("module '%s' help message\\n",THIS_MODULE->name);
      return 1;
    }
  }
}

void MOD_NAME_module_init(int argc, char *argv[])
{
  MOD_NAME_pre_init(argc,argv);

  MOD_NAME_dump_params();

  register_module(THIS_MODULE);
}
//MODULE_INIT(MOD_NAME_module_init)

void MOD_NAME_module_exit()
{
  MOD_NAME_release();
}
//MODULE_EXIT(MOD_NAME_module_exit)
"""
  

  #########################
  #
  # header file template
  #
  ########################
  header_template = """
#ifndef {0}
# define {0}



#endif  /* {0} */
"""
  

  #########################
  #
  # Makefile template
  #
  ########################
  makefile_template = """
FRAMEWORK_SRC := ../../

include $(FRAMEWORK_SRC)/rules.mak

MODULE_NAME      := MOD_NAME

C_SRCS        := $(shell find ./ -name "*.c" )
C_OBJS        := $(patsubst %.c,%.o,$(C_SRCS))

OBJS            := $(C_OBJS)

LDFLAG          :=  
CFLAG_OBJS      := -Wall -Werror -I. -g  -fPIC -Wno-error=unused-function 
CFLAG_OBJS      += -I$(FRAMEWORK_SRC) -I$(FRAMEWORK_SRC)/lib/ 


# TODO: your own ldflags here
EXTRA_LDFLAG    := 
LDFLAG          += $(EXTRA_LDFLAG)

# TODO: your own cflags here
EXTRA_CFLAG     :=
CFLAG_OBJS      += $(EXTRA_CFLAG)


TARGET_SO       := $(MODULE_NAME)$(DY_MOD_EXT)


.PHONY: all
all: $(TARGET_SO) 

$(TARGET_SO):$(OBJS)
	$(CC) $(OBJS) $(LDFLAG) -shared -o $@

$(C_OBJS):%.o:%.c
	$(CC) $(CFLAG_OBJS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET_SO) 
"""



  def __init__(self,mod_name):
    self.module_name = mod_name



  #
  # main entry to generate the moudle source file
  #
  def do_gen_source(self):

    # generate the c file
    outfilename = self.module_name + ".c"
    m_fd = open(outfilename,"w")

    s = self.c_file_template.replace('MOD_NAME',self.module_name)

    print(s,file=m_fd)

    m_fd.close()


    # generate the header file
    outfilename = self.module_name + ".h"
    m_fd = open(outfilename,"w")
    ptn =  "__"  +  outfilename.replace('./','').replace('.','_').upper() + "__"

    print(self.header_template.format(ptn),file=m_fd)

    m_fd.close()




  #
  # generate the moudle Makefile file
  #
  def do_gen_makefile(self):

    outfilename = "Makefile"
    m_fd = open(outfilename,"w")


    s = self.makefile_template.replace('MOD_NAME',self.module_name)

    print(s,file=m_fd)

    m_fd.close()



def help():
  print("Generate .c/.h source & Makefile templates\n")
  print("Format: {0} <module name>".format(__file__))
  exit(-1)
 



def main():

  if len(argv)==2:
    script,modName = argv

    if modName=='-h':
      help()

  else:
    print("Not enough arguments\n")
    help()


  mg = ModTemplateGenerator(modName)
  mg.do_gen_source()
  mg.do_gen_makefile()

  exit(0)



if __name__ == "__main__":
  main()

