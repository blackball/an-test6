/*
   File:        backtrace.c
   Author:      Paul Komarek
   Created:     Sun Oct 20 22:57:35 EDT 2002
   Description: Facilities to produce backtraces from running code.

   Copyright 2002, AUTON Lab, Carnegie Mellon University.
*/


/* As of Sun Oct 20 22:57:35 EDT 2002, the backtrace functionality for
   GNU CC combined with GLIBC only returns function names from shared
   objects (you can get addresses from statically linked stuff).
   I have added a btdebug compile type to Make.common which does the right
   stuff.  To get the most from these backtrace utiltities (at least
   under GNU CC and GLIBC), compile with "gmake t=btdebug".

   Sometime later, the default debug target mimicked btdebug.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "backtrace.h"
#include "am_string.h"
#include "amma.h"

char *mk_parse_funcname_gcc_glibc_i386( const char *funcname);
/* This is where we store the old sigsegv handler if
   backtrace_install_sigsegv_handler() is called, as well as for
   "...sigusr2...".*/
void (*backtrace_old_sigsegv_handler)(int);
void (*backtrace_old_sigusr2_handler)(int);

btinfo *mk_btinfo( int maxdepth)
{
  btinfo *bt;

  bt = AM_MALLOC( btinfo);
  bt->maxdepth  = maxdepth;
  bt->funcnames = NULL;

  /* fill_btinfo() is a macro defined according to platform. */
  /* This function is not prefixed by mk_ because freeing the funcnames
     is handled by mk_btinfo(). */
  fill_btinfo_funcnames( bt);

  /* If bt->funcnames hasn't been filled in, warn the user that
     our backtrace functionality isn't available on this platform
     or build. */
  if (bt->funcnames == NULL) {
    fprintf( stderr,
	     "\nmk_btinfo: Warning: "
	     "mk_btinfo unimplemented for this build or platform.\n\n");
  }

  return bt;
}

void fprintf_btinfo( FILE *f, char *preline, btinfo *bt, char *postline)
{
  int i, num;
  char *s;

  /* Protect ourselves against incautious callers. */
  if (bt == NULL || bt->funcnames == NULL) return;

  num = string_array_size( bt->funcnames) - 1;

  fprintf( f, preline);
  fprintf( f, "Call Stack (most recent call at bottom):\n");
  for (i = num; i>=0; --i) {
    s = string_array_ref( bt->funcnames, i);
    fprintf( f, "%d: %s\n", i, s);
  }
  fprintf( f, postline);
  return;
}

void free_btinfo( btinfo *bt)
{
  if (bt != NULL) {
    if (bt->funcnames != NULL) free_string_array( bt->funcnames);
    AM_FREE( bt, btinfo);
  }
  return;
}

/***********************************************************************/
/* Signal Catching Helpers                                             */
/***********************************************************************/

void backtrace_sigsegv( int sig)
{
  btinfo *bt;
  void (*sigrc)(int);

  /* Restore default SIGSEGV handler in case we produce another segfault. */
  sigrc = signal( SIGSEGV, backtrace_old_sigsegv_handler);
  if (sigrc == SIG_ERR) {
    fprintf( stderr, "Failed to reinstall old SIGSEGV handler.\n");
    exit(1);
  }
  else if (sigrc != backtrace_sigsegv) {
    fprintf( stderr, "Strangely, the old SIGSEGV handler is not me.\n");
    exit(2);
  }

  /* Print advisory message. */
  fprintf( stderr, "\nSegfault (caught by backtrace_sigsegv)\n");

  /* Get and print backtrace. */
  bt = mk_btinfo( 15);
  if (bt != NULL && bt->funcnames != NULL) {
    fprintf_btinfo( stderr, "\n", bt, "\n");
  }
  if (bt != NULL) free_btinfo( bt);

  /* Call old signal handler. */
  /* Tru64 acts funny when the old handler is called.  It seems to be
     busy-waiting inside the old signal handler.  I've given up figuring
     out why, or what I can do about it with ANSI C.  I am starting to
     think that I should switch to POSIX semantics and create an
     am_sigaction() wrapper to hide the POSIX stuff from Windows.
  */
  fprintf( stderr, "Calling old signal handler.  If this fails to achieve the "
	   "expected results,\nuse ctrl-c to terminate the program and "
	   "consider disabling any calls to\n"
	   "backtrace_install_sigsegv_handler() or otherwise installing\n"
	   "backtrace_sigsegv() as a signal handler.\n\n");
  backtrace_old_sigsegv_handler( sig);

  /* If the old signal handler returns, then we should, too. */
  return;
}

void backtrace_install_sigsegv_handler( void)
{
  /* Only install signal handler if we're doing BTDEBUG builds.  This
     reduces the chance of screwing up a core dump when the user doesn't
     even want backtrace functionality. */

#ifdef BTDEBUG
  /* BTDEBUG is defined. */
  /* Before installing handler, check if this platform is supported. */
  {
    btinfo *bt;
    bt = mk_btinfo( 1);
    if (bt == NULL || bt->funcnames == NULL) {
      fprintf( stderr, "backtrace_install_sigsegv_handler: handler not "
	       "installed because backtrace functionality\nis not supported "
	       "for this build/platform.\n");
    }
    else {
      if (signal(SIGSEGV, SIG_IGN) != SIG_IGN) {
	backtrace_old_sigsegv_handler = signal( SIGSEGV, backtrace_sigsegv);
      }
      else my_error( "backtrace_install_syssegv_handler: handler was not "
		     "installed, because\nSIGSEGV is ignored.\n");
    }
    free_btinfo( bt);
  }

#else
  /* BTDEBUG *not* defined. */
  fprintf( stderr, "backtrace_install_sigsegv_handler: handler not installed "
	   "because BTDEBUG\nis not defined.\n");
#endif

  return;
}

void backtrace_sigusr2( int sig)
{	/* Windows can't even think about SIGUSR2 safely */
#ifdef BTDEBUG
  btinfo *bt;
  void (*sigrc)(int);

  /* Reinstall us as the signal handler. */
  sigrc = signal( SIGUSR2, backtrace_sigusr2);
  if (sigrc == SIG_ERR) {
    fprintf( stderr, "Failed to reinstall SIGUSR2 handler.\n");
    exit(1);
  }
  else if (sigrc != backtrace_sigusr2) {
    fprintf( stderr, "Strangely, the old SIGUSR2 handler is not me.\n");
    exit(2);
  }

  /* Print advisory message. */
  fprintf( stderr, "\nSIGUSR2 (caught by backtrace_sigusr2)\n");

  /* Get and print backtrace. */
  bt = mk_btinfo( 15);
  if (bt != NULL && bt->funcnames != NULL) {
    fprintf_btinfo( stderr, "\n", bt, "\n");
  }
  if (bt != NULL) free_btinfo( bt);

  return;
#endif /* BTDEBUG */
}

void backtrace_install_sigusr2_handler( void)
{
  /* Only install signal handler if we're doing BTDEBUG builds.  This
     reduces the chance of screwing up a core dump when the user doesn't
     even want backtrace functionality. */

#ifdef BTDEBUG
  /* BTDEBUG is defined. */
  /* Before installing handler, check if this platform is supported. */
  {
    btinfo *bt;
    bt = mk_btinfo( 1);
    if (bt == NULL || bt->funcnames == NULL) {
      fprintf( stderr, "backtrace_install_sigusr2_handler: handler not "
	       "installed because backtrace functionality\nis not supported "
	       "for this build/platform.\n");
    }
    else {
      if (signal(SIGUSR2, SIG_IGN) != SIG_IGN) {
	backtrace_old_sigusr2_handler = signal( SIGUSR2, backtrace_sigusr2);
      }
      else my_error( "backtrace_install_sysusr2_handler: handler was not "
		     "installed, because\nSIGUSR2 is ignored.\n");
    }
    free_btinfo( bt);
  }

#else
  /* BTDEBUG *not* defined. */
  fprintf( stderr, "backtrace_install_sigusr2_handler: handler not installed "
	   "because BTDEBUG\nis not defined.\n");
#endif

  return;
}

/***********************************************************************/
/* Messy fill_btinfo_funcnames() Switch                                */
/***********************************************************************/

void fill_btinfo_funcnames( btinfo *bt)
{
  bt->funcnames = NULL;

#ifdef __GNUC__
#ifdef __GLIBC__
#if __i386__ || __x86_64__
  fill_btinfo_funcnames_gcc_glibc_i386( bt);
#endif
#endif
#endif

  return;
}

/***********************************************************************/
/* GCC + GLIBC                                                         */
/***********************************************************************/

/* This function cleans up the names returned by backtrace_symbols. */
char *mk_parse_funcname_gcc_glibc_i386( const char *funcname)
{
  char *tmp, *pos1, *pos2, *result;

  /* Input should look something like one of
       "./bt.so(main+0xb) [0x2aac2573]"
       "/lib/libc.so.6(__libc_start_main+0xbd) [0x2aaec671]"
  */


  /* Find beginning of function name. */
  tmp = mk_copy_string( funcname);
  pos1 = strchr( tmp, '(');   /* Beginning of (foo+bar). */
  if (pos1 == NULL) return tmp;

  /* Find end of function name. */
  pos2 = strchr( pos1+1, '+');   /* End of function name. */
  if (pos2 == NULL) return tmp;

  /* Copy string starting at pos1+1 up to and including pos2. */
  *pos2 = '\0';
  
  result = mk_copy_string( pos1+1);
  *pos2 = '+';

  /* Done. */
  free_string( tmp);
  return result;
}

#ifdef __GNUC__
#ifdef __GLIBC__
#if __i386__ || __x86_64__
void fill_btinfo_funcnames_gcc_glibc_i386( btinfo *bt)
{
  int num, i;
  void **addresses;
  char **funcnames, *funcname, *newname;

  /* Get backtrace.  This is the system dependent part. */
  addresses = AM_MALLOC_ARRAY( void *, bt->maxdepth);
  num       = backtrace( addresses, bt->maxdepth);
  funcnames = backtrace_symbols( addresses, num);

  /* backtrace() does not appear to actually allocate anything; that is
     if you try freeing it's elements then you will segfault. */
  AM_FREE_ARRAY( addresses, void *, bt->maxdepth);

  /* Fill in btinfo struct. */
  bt->funcnames = mk_string_array( num);
  for (i=0; i<num; ++i) {
    funcname = funcnames[i];
    newname = mk_parse_funcname_gcc_glibc_i386( funcname);
    string_array_set( bt->funcnames, i, newname);
    free_string( newname);
  }

  /* funcnames is allocated as a block, therefore we only need to call
     free once. */
  free( funcnames);

  return;
}
#endif
#endif
#endif
