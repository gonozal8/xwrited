/*
 * Copyright (C) 2010 Guido Berhoerster <guido+xwrited@berhoerster.name>
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

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <utmpx.h>
#include <errno.h>
#include <sys/time.h>

#ifndef	DEV_PREFIX
#define	DEV_PREFIX	"/dev/"
#endif /* !DEV_PREFIX */

static void
utmp_write_entry(int fd, gboolean add)
{
	struct utmpx	utmpx;
	char		*line = NULL;
	size_t		line_len;
	char		*id;
	struct passwd	*pwd;

	line = ptsname(fd);
	if (line == NULL) {
		g_critical("failed to obtain slave pty name");
		return;
	}
	if (g_str_has_prefix(line, DEV_PREFIX)) {
		line += strlen(DEV_PREFIX);
	}

	line_len = strlen(line);
	id = (line_len >= sizeof (utmpx.ut_pid)) ?
	    line + (line_len - sizeof (utmpx.ut_pid)) :
	    line;

	pwd = getpwuid(getuid());
	if (pwd == NULL) {
		g_critical("failed to get username: %s", g_strerror(errno));
		return;
	}

	memset(&utmpx, 0, sizeof (utmpx));
	strncpy(utmpx.ut_name, pwd->pw_name, sizeof (utmpx.ut_name));
	strncpy(utmpx.ut_id, id, sizeof (utmpx.ut_id));
	strncpy(utmpx.ut_line, line, sizeof (utmpx.ut_line));
	utmpx.ut_pid = getpid();
	utmpx.ut_type = add ? USER_PROCESS : DEAD_PROCESS;
	gettimeofday(&utmpx.ut_tv, NULL);

	setutxent();
	if (pututxline(&utmpx) == NULL) {
		g_critical("failed to write to utmpx database: %s",
		    g_strerror(errno));
		return;
	}
	endutxent();
}

void
xwrited_utmp_add_entry(int fd)
{
	utmp_write_entry(fd, TRUE);
}

void
xwrited_utmp_remove_entry(int fd)
{
	utmp_write_entry(fd, FALSE);
}
