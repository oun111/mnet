#include <stdio.h>
#include <string.h>
#include "instance.h"




int main(int argc, char *argv[])
{
#if 0
  instance_start(argc,argv);
#else
  {
    extern void test_jsons();
    test_jsons();
  }
#endif

  return 0;
}

