#include <stdio.h>
#include <string.h>
//#include <getopt.h>
#include "instance.h"




int main(int argc, char *argv[])
{
#if 1
  instance_start(argc,argv);
#else
  {
    extern void test_ssl();
    test_ssl();
  }
#endif

  return 0;
}

