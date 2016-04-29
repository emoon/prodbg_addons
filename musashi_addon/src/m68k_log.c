#include "m68k_log.h"
#include <stdio.h>
#include <stdarg.h>

static int s_log_level = 0;
static int s_old_level = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void m68k_log(int logLevel, const char* format, ...)
{
	va_list ap;

	if (logLevel < s_log_level)
		return;

	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void m68k_log_set_level(int logLevel)
{
    s_log_level = logLevel;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void m68k_log_level_push()
{
    s_old_level = s_log_level;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void m68k_log_level_pop()
{
    s_log_level = s_old_level;
}
