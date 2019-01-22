#include "jamvm_wrapper.h"
#include "jam.h"
#include "hash.h"
#include "class.h"
#include "symbol.h"
#include "excep.h"
#include "thread.h"
#include "classlib.h"
#include <linux/limits.h>
#include <alloca.h>
#include <string.h>
#include "dbuffer.h"
#include "log.h"



/*
 * the reference command line of jamvm without gnu-classpath configured is :
 *
 * ./jamvm   \
 *
 *   -Xbootclasspath/c:$(gnu classpath install dir)/share/classpath/glibj.zip  \
 *
 *   -Dgnu.classpath.boot.library.path=$(gnu classpath install dir)/lib/classpath  \
 *
 *   -cp ../java/ zipTest  
 *
 */

int 
jamvm_wrapper_run(const char *gnuclasspath, const char *webapppath, const char *appclassname)
{
  InitArgs args;
  Object *system_loader, *array;
  Class *array_class, *main_class;
  MethodBlock *mb;
  int status = 0 ;

  const char *strBootLibPath = "gnu.classpath.boot.library.path";
  char *boot_lib_path = NULL ;
  dbuffer_t blp = NULL ;


  setDefaultInitArgs(&args);

  // the '-Xbootclasspath/c:' argument
  args.bootpath_c = alloc_dbuffer(PATH_MAX);
  snprintf(args.bootpath_c,PATH_MAX,"%s/share/classpath/glibj.zip",gnuclasspath);


  // the '-Dgnu.classpath.boot.library.path' argument
  boot_lib_path = sysMalloc(strlen(strBootLibPath)+2);
  strcpy(boot_lib_path,strBootLibPath);
  
  blp = alloc_dbuffer(PATH_MAX);
  snprintf(blp,PATH_MAX,"%s/lib/classpath/",gnuclasspath);

  args.commandline_props = sysMalloc(sizeof(Property));
  args.commandline_props[0].key   = (char*)boot_lib_path ;
  args.commandline_props[0].value = blp ;
  args.props_count++ ;


  // the '-cp' argument
  args.classpath = (char*)webapppath ;

  // verbose options
  //args.verboseclass = TRUE; 
  //args.verbosedll = TRUE; 

  // initialize 
  if (!initVM(&args)) {
    log_error("fatal: init vm fail!\n");
    goto __done ;
  }

  if (!(system_loader = getSystemClassLoader())) {
    log_error("fatal: get system class loader!\n");
    goto __done ;
  }

  mainThreadSetContextClassLoader(system_loader);  

  main_class = findClassFromClassLoader((char*)appclassname, system_loader);
  if (!main_class) {
    log_error("fatal: main class not found!\n");
    goto __done ;
  }

  initClass(main_class);

  mb = lookupMethod(main_class, SYMBOL(main),
                                SYMBOL(_array_java_lang_String__V));

  if(mb == NULL || !(mb->access_flags & ACC_STATIC)) {
    signalException(java_lang_NoSuchMethodError, "main");
    goto __done;
  }


#if 0
  int i=0;
  int argc = 6;

	i = 5 + 1;
#endif
	if((array_class = findArrayClass(SYMBOL(array_java_lang_String))) &&
				 (array = allocArray(array_class, /*argc - i*/0, sizeof(Object*))))  {
#if 0
			Object **args = ARRAY_DATA(array, Object*) - i;

			for(; i < argc; i++)
					if(!(args[i] = Cstr2String(/*argv[i]*/"arg111")))
							break;

			/* Call the main method */
			if(i == argc)
#endif
					executeStaticMethod(main_class, mb, array);
	}


__done:
  drop_dbuffer(args.bootpath_c);
  drop_dbuffer(blp);

  /* Wait for all but daemon threads to die */
#if 0
  mainThreadWaitToExitVM();
  exitVM(status);
#else
  (void)status;
#endif

  return 0;
}

