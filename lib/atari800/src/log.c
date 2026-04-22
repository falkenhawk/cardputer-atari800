/*
 * log.c — cardputer-atari800 embedded build.
 * Replaces upstream's 8 KB stack-allocated buffer (which blows the 8 KB
 * loopTask stack on the first Log_print call) with a small static buffer
 * routed through the port's port_log_write hook (defined in port_impl.cpp).
 *
 * This is the patch the M2 plan's T5 described but never applied — upstream
 * log.c compiled and linked clean, so the bomb stayed latent until a code
 * path that actually calls Log_print (e.g. binload.c's "can't open" error
 * when the xex is missing) was exercised.
 *
 * Licensing follows upstream (GPLv2); this file replaces upstream's log.c
 * in the vendored src/ subset only.
 */

#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

extern void port_log_write(const char *msg);

void Log_print(const char *format, ...)
{
	/* Static buffer — thread-safe because we only call Log_print from the
	   main loopTask. 256 bytes is plenty for any upstream log message
	   (most are < 80 chars). Truncation is acceptable here. */
	static char buffer[256];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer) - 2, format, args);
	va_end(args);
	strcat(buffer, "\n");
	port_log_write(buffer);
}

void Log_flushlog(void)
{
	/* no-op — buffering is handled by the port (Serial stdout) */
}
