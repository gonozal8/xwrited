/*
 * Copyright (C) 2014 Guido Berhoerster <guido+xwrited@berhoerster.name>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <string.h>
#include <stdarg.h>
#include <glib.h>

#include "xwrited-debug.h"

#if !GLIB_CHECK_VERSION(2, 32, 0)
static void
dummy_log_handler(const gchar *log_domain, GLogLevelFlags log_level,
    const gchar *message, gpointer data)
{
	/* drop all messages */
}
#endif /* !GLIB_CHECK_VERSION (2,32,0) */

void
xwrited_debug_init(gboolean debug_mode)
{
	/*
	 * glib >= 2.32 only shows debug messages if the G_MESSAGES_DEBUG
	 * environment variable contains the log domain or "all", earlier glib
	 * version always show debugging output
	 */
#if GLIB_CHECK_VERSION(2, 32, 0)
	const gchar	*debug_env;
	gchar		*debug_env_new;

	if (debug_mode) {
		debug_env = g_getenv("G_MESSAGES_DEBUG");

		if (debug_env == NULL) {
			g_setenv("G_MESSAGES_DEBUG", G_LOG_DOMAIN, TRUE);
		} else if (strstr(debug_env, G_LOG_DOMAIN) == NULL) {
			debug_env_new = g_strdup_printf("%s %s", debug_env,
			    G_LOG_DOMAIN);
			g_setenv("G_MESSAGES_DEBUG", debug_env_new, TRUE);
			g_free(debug_env_new);
		}
	}
#else
	if (!debug_mode) {
		g_log_set_handler(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
		    dummy_log_handler, NULL);
	}
#endif /* GLIB_CHECK_VERSION (2,32,0) */
}
