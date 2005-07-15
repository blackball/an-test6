/* am_time_private.h */
/* No #ifdef protection, because it should only be included from one place anyhow */

  /* itimer stuff checks once an hour */
#define AUTON_IT_CONSTANT (60*60)





#ifdef UNIX_PLATFORM
struct itimer_state
{
  int signal_used; /* Non-active data, knows what signal is used for this itimer*/
  int iter_counter; /* Active data, holds number of iterations we've gone thr*/
  struct itimerval itim;
  char name[12];
  int active; /* 1 = inactive, 2 = active. 0 = try to catch uninitialized.*/
};

static void itimer_sig_hdlr(int sig_id);
static int sigid_to_index(int sig_id);
static int itimet_to_index(int itimer_type);

#endif /* UNIX_PLATFORM */


static void void_free_date(void* data);
static void *void_mk_copy_date(void* data);
static void void_fprintf_date(FILE* s, char* m1, void* data, char* m2);

