#include <stdarg.h>
#include "vt100/vt100.h"

FILE *debug = NULL;
static SDL_SpinLock lock_logger;

void logger (const char *device, const char *format, ...)
{
  va_list ap;
  if (debug == NULL)
    return;
  SDL_AtomicLock (&lock_logger);
  fprintf (debug, "[LOG %12llu %-4s | ", get_cycles (), device);
  va_start (ap, format);
  vfprintf (debug, format, ap);
  fprintf (debug, "]\n");
  va_end (ap);
  SDL_AtomicUnlock (&lock_logger);
}
