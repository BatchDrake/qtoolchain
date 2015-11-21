#include <stdarg.h>

#include "q_defines.h"
#include "q_util.h"

static char last_error[QSPARSE_LAST_ERROR_MAX];

void
q_set_last_error (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);

  vsnprintf (last_error, QSPARSE_LAST_ERROR_MAX, fmt, ap);

  va_end (ap);
}

const char *
q_get_last_error (void)
{
  return last_error;
}
