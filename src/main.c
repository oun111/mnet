#include <stdio.h>
#include <string.h>
#include "instance.h"




int main(int argc, char *argv[])
{
#if 1
  instance_start(argc,argv);
#else
  {
    extern void test_crypto();
    test_crypto();
  }
#endif

  return 0;
}

