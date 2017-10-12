/*
 * Copyright (C) 2015 Guido Berhoerster <guido+xwrited@berhoerster.name>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <locale.h>
#include <libintl.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <libnotify/notify.h>
#include "xwrited-debug.h"
#include "xwrited-unique.h"
#include "xwrited-utmp.h"

#define BUFFER_TIMEOUT (250)

enum {
	PIPE_R_FD = 0,
	PIPE_W_FD
};

static int		signal_pipe_fd[2] = { -1, -1 };
static guint		notify_timeout_id;
static GMainLoop	*loop;
static GString		*buffer;

static void
on_signal(int signo)
{
	int		old_errno = errno;
	ssize_t		n;
	sigset_t	sigset;

	/* try to read unread signals from the pipe and add the new one to it */
	n = read(signal_pipe_fd[PIPE_R_FD], &sigset, sizeof (sigset));
	if ((n == -1) || ((size_t)n < sizeof (sigset))) {
		sigemptyset(&sigset);
	}
	sigaddset(&sigset, signo);
	write(signal_pipe_fd[PIPE_W_FD], &sigset, sizeof (sigset));

	errno = old_errno;
}

static gboolean
signal_read_cb(GIOChannel *source, GIOCondition cond, gpointer user_data)
{
	sigset_t	sigset;
	sigset_t	old_sigset;
	GIOStatus	status;
	gsize		n;
	GError		*error = NULL;

	/*
	 * deal with pending signals previously received in the signal handler,
	 * try to read a sigset from the pipe, avoid partial reads by blocking
	 * all signals during the read operation
	 */
	sigfillset(&sigset);
	sigprocmask(SIG_BLOCK, &sigset, &old_sigset);
	status = g_io_channel_read_chars(source, (gchar *)&sigset,
	    sizeof (sigset), &n, &error);
	sigprocmask(SIG_SETMASK, &old_sigset, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		if (status != G_IO_STATUS_AGAIN) {
			if (error != NULL) {
				g_critical("failed to read from signal pipe: "
				    "%s", error->message);
				g_error_free(error);
				g_main_loop_quit(loop);
			} else {
				g_critical("failed to read from signal pipe");
				g_main_loop_quit(loop);
			}
		}
	} else if (n != sizeof (sigset)) {
		g_critical("short read from signal pipe");
		g_main_loop_quit(loop);
	} else {
		if ((sigismember(&sigset, SIGINT) == 1) ||
		    (sigismember(&sigset, SIGTERM) == 1) ||
		    (sigismember(&sigset, SIGQUIT) == 1) ||
		    (sigismember(&sigset, SIGHUP) == 1)) {
			g_debug("received signal, exiting");
			g_main_loop_quit(loop);
		}
	}

	return (TRUE);
}

static gboolean
send_notification(void)
{
	gboolean	retval = FALSE;
	GString		*utf8_str = NULL;
	gchar		*startp = buffer->str;
	gchar		*endp;
	GRegex		*regex = NULL;
	GError		*error = NULL;
	gchar		*body = NULL;
	GList		*capabilities = NULL;
	gchar		*tmp;
	NotifyNotification *notification = NULL;

	utf8_str = g_string_sized_new(buffer->len);
	while (!g_utf8_validate(startp, buffer->str + buffer->len -
	    startp, (const gchar **)&endp)) {
		g_string_append_len(utf8_str, startp, endp - startp);
		/*
		 * replace each byte that does not belong to a UTF-8-encoded
		 * character with the Unicode REPLACEMENT CHARACTER (U+FFFD)
		 */
		g_string_append(utf8_str, "\357\277\275");

		startp = endp + ((endp < buffer->str + buffer->len) ? 1 : 0);
	}
	g_string_append_len(utf8_str, startp, buffer->str + buffer->len -
	    startp);

	/* remove any CR, BEL and trailing space and tabs */
	regex = g_regex_new("([\r\a]+|[ \t\r\a]+$)", G_REGEX_MULTILINE, 0,
	    &error);
	if (error != NULL) {
		g_critical("failed to create regex object: %s",
		    error->message);
		g_error_free(error);
		goto out;
	}
	body = g_regex_replace_literal(regex, utf8_str->str, -1, 0, "", 0,
	    &error);
	if (error != NULL) {
		g_critical("failed to replace control and space characters: "
		    "%s", error->message);
		g_error_free(error);
		goto out;
	}

	/*
	 * skip empty messages or messages only consisting of whitespace and
	 * control characters
	 */
	if ((strlen(body) == 0) ||
	    !g_regex_match_simple("[^[:space:][:cntrl:]]", body, 0, 0)) {
		retval = TRUE;
		goto out;
	}

	/*
	 * if the notification daemon supports markup the message needs to be
	 * escaped
	 */
	capabilities = notify_get_server_caps();
	if (g_list_find_custom(capabilities, "body-markup",
	    (GCompareFunc)strcmp) != NULL) {
		tmp = g_markup_escape_text(body, -1);
		g_free(body);
		body = tmp;
	}

	/* show notification */
	notification = notify_notification_new(_("Message received"),
	    body, "utilities-terminal"
#if !defined(NOTIFY_VERSION_MINOR) || \
    (NOTIFY_VERSION_MAJOR == 0 && NOTIFY_VERSION_MINOR < 7)
	    , NULL
#endif
	    );
	if (notification == NULL) {
		g_critical("failed to create a notification object");
		g_main_loop_quit(loop);
		goto out;
	}
	notify_notification_set_timeout(notification, NOTIFY_EXPIRES_NEVER);
	retval = notify_notification_show(notification, NULL);

out:
	if (notification != NULL) {
		g_object_unref(G_OBJECT(notification));
	}
	if (capabilities != NULL) {
		g_list_free_full(capabilities, g_free);
	}
	g_free(body);
	if (regex != NULL) {
		g_regex_unref(regex);
	}
	if (utf8_str != NULL) {
		g_string_free(utf8_str, TRUE);
	}
	/* prevent a permanently large buffer */
	g_string_free(buffer, FALSE);
	buffer = g_string_sized_new(BUFSIZ);

	return (retval);
}

static gboolean
notify_timeout_cb(gpointer user_data)
{
	if (!send_notification()) {
		g_warning("failed to send notification");
	}

	notify_timeout_id = 0;

	return (FALSE);
}

static gboolean
master_pty_read_cb(GIOChannel *source, GIOCondition cond, gpointer user_data)
{
	gchar		buf[BUFSIZ];
	GIOStatus	status;
	gsize		buf_len;
	GError		*error = NULL;

	if ((cond & G_IO_IN) || (cond & G_IO_PRI)) {
		/* read message from master pty */
		while ((status = g_io_channel_read_chars(source, buf, BUFSIZ,
		    &buf_len, &error)) == G_IO_STATUS_NORMAL) {
			if (buf_len > 0) {
				g_debug("read %" G_GSSIZE_FORMAT " bytes from "
				    "master pty", buf_len);
				g_string_append_len(buffer, buf,
				    (gssize)buf_len);
			}
		}
		if (error != NULL) {
			g_critical("failed to read from master pty: %s",
			    error->message);
			g_error_free(error);
			g_main_loop_quit(loop);
			return (FALSE);
		}

		/*
		 * schedule a timeout for sending a notification with the
		 * buffered message
		 */
		if (notify_timeout_id <= 0) {
			notify_timeout_id = g_timeout_add(BUFFER_TIMEOUT,
			    notify_timeout_cb, NULL);
		}
	}

	if ((cond & G_IO_ERR) || (cond & G_IO_HUP)) {
		g_critical("connection to master pty broken");
		g_main_loop_quit(loop);
		return (FALSE);
	}

	return (TRUE);
}

int
main(int argc, char *argv[])
{
	int		status = EXIT_FAILURE;
	GError		*error = NULL;
	XWritedUnique	*app = NULL;
	GOptionContext	*context = NULL;
	struct sigaction sigact;
	GIOChannel	*signal_channel = NULL;
	GIOChannel	*master_pty_channel = NULL;
	int		masterfd = -1;
	int		slavefd = -1;
	char		*slave_name = NULL;
	gboolean	vflag = FALSE;
	gboolean	dflag = FALSE;
	const GOptionEntry options[] = {
	    { "debug", 'd', 0, G_OPTION_ARG_NONE, &dflag,
	    N_("Show extra debugging information"), NULL },
	    { "version", 'V', 0, G_OPTION_ARG_NONE, &vflag,
	    N_("Print the current version and exit"), NULL },
	    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);

#if !GLIB_CHECK_VERSION(2, 35, 0)
	/* deprecated in glib >= 2.35 */
	g_type_init();
#endif

	context = g_option_context_new("- display write and wall messages as "
	    "desktop notifications");
	g_option_context_add_main_entries(context, options, PACKAGE);
	g_option_context_set_translation_domain(context, PACKAGE);
	g_option_context_parse(context, &argc, &argv, &error);
	if (error != NULL) {
		g_printerr("%s.\n", error->message);
		g_error_free(error);
		goto out;
	}

	xwrited_debug_init(dflag);

	if (vflag) {
		g_print("%s %s\n", PACKAGE, VERSION);
		status = EXIT_SUCCESS;
		goto out;
	}

	app = xwrited_unique_new("org.guido-berhoerster.code.xwrited");
	if (app == NULL) {
		g_critical("failed to initialize application");
		goto out;
	}
	if (!xwrited_unique_is_unique(app)) {
		g_printerr(_("xwrited is already running in this session.\n"));
		goto out;
	}

	if (!notify_init(APP_NAME)) {
		g_critical("failed to initialize libnotify");
		goto out;
	}

	loop = g_main_loop_new(NULL, FALSE);
	if (loop == NULL) {
		g_critical("failed to create main loop");
		goto out;
	}

	buffer = g_string_sized_new(BUFSIZ);

	/* open master pty */
	masterfd = posix_openpt(O_RDWR | O_NOCTTY);
	if (masterfd == -1) {
		g_critical("failed to open master pty: %s", g_strerror(errno));
		goto out;
	}

	/* create slave pty */
	if ((grantpt(masterfd) == -1) || (unlockpt(masterfd) == -1)) {
		g_critical("failed to create slave pty: %s", g_strerror(errno));
		goto out;
	}
	slave_name = ptsname(masterfd);
	if (slave_name == NULL) {
		g_critical("failed to obtain name of slave pty");
		goto out;
	}

	/*
	 * keep an open fd around order to prevent closing the master fd when
	 * receiving an EOF
	 */
	slavefd = open(slave_name, O_RDWR);
	if (slavefd == -1) {
		g_critical("failed to open slave pty: %s", g_strerror(errno));
		goto out;
	}

	/* create a GIOChannel for monitoring the master pty for messages */
	master_pty_channel = g_io_channel_unix_new(masterfd);
	g_io_channel_set_flags(master_pty_channel,
	    g_io_channel_get_flags(master_pty_channel) | G_IO_FLAG_NONBLOCK,
	    &error);
	if (error != NULL) {
		g_critical("failed set flags on the master pty channel: %s",
		    error->message);
		g_error_free(error);
		goto out;
	}
	if (!g_io_add_watch(master_pty_channel, G_IO_IN | G_IO_PRI | G_IO_HUP |
	    G_IO_ERR, master_pty_read_cb, NULL)) {
		g_critical("failed to add watch on signal channel");
		goto out;
	}

	/* create pipe for delivering signals to a listener in the main loop */
	if (pipe(signal_pipe_fd) == -1) {
		g_critical("failed to create signal pipe: %s",
		    g_strerror(errno));
		goto out;
	}
	if (fcntl(signal_pipe_fd[PIPE_W_FD], F_SETFL, O_NONBLOCK) == -1) {
		g_critical("failed to set flags on signal pipe: %s",
		    g_strerror(errno));
		goto out;
	}

	/* create GIO channel for reading from the signal_pipe */
	signal_channel = g_io_channel_unix_new(signal_pipe_fd[PIPE_R_FD]);
	g_io_channel_set_encoding(signal_channel, NULL, &error);
	if (error != NULL) {
		g_critical("failed to set binary encoding for signal channel: "
		    "%s", error->message);
		g_error_free(error);
		goto out;
	}
	g_io_channel_set_buffered(signal_channel, FALSE);
	g_io_channel_set_flags(signal_channel,
	    g_io_channel_get_flags(signal_channel) | G_IO_FLAG_NONBLOCK,
	    &error);
	if (error != NULL) {
		g_critical("failed set flags on signal channel: %s",
		    error->message);
		g_error_free(error);
		goto out;
	}
	if (g_io_add_watch(signal_channel, G_IO_IN | G_IO_PRI | G_IO_HUP |
	    G_IO_ERR, signal_read_cb, NULL) == 0) {
		g_critical("failed to add watch on the signal channel");
		goto out;
	}

	/* set up signal handler */
	sigact.sa_handler = on_signal;
	sigact.sa_flags = SA_RESTART;
	sigemptyset(&sigact.sa_mask);
	if ((sigaction(SIGINT, &sigact, NULL) < 0) ||
	    (sigaction(SIGTERM, &sigact, NULL) < 0) ||
	    (sigaction(SIGQUIT, &sigact, NULL) < 0) ||
	    (sigaction(SIGHUP, &sigact, NULL) < 0)) {
		g_critical("failed to set up signal handler");
		goto out;
	}

	xwrited_utmp_add_entry(masterfd);

	/* main loop */
	g_main_loop_run(loop);

	xwrited_utmp_remove_entry(masterfd);

	status = EXIT_SUCCESS;

out:
	if (context != NULL) {
		g_option_context_free(context);
	}

	if (signal_channel != NULL) {
		g_io_channel_shutdown(signal_channel, FALSE, NULL);
		g_io_channel_unref(signal_channel);
	}

	if (signal_pipe_fd[PIPE_R_FD] != -1) {
		close(signal_pipe_fd[PIPE_R_FD]);
	}
	if (signal_pipe_fd[PIPE_W_FD] != -1) {
		close(signal_pipe_fd[PIPE_W_FD]);
	}

	if (master_pty_channel != NULL) {
		g_io_channel_shutdown(master_pty_channel, FALSE, NULL);
		g_io_channel_unref(master_pty_channel);
	}

	if (slavefd != -1) {
		close(slavefd);
	}

	if (masterfd != -1) {
		close(masterfd);
	}

	if (buffer != NULL) {
		g_string_free(buffer, FALSE);
	}

	if (app != NULL) {
		g_object_unref(app);
	}

	if (loop != NULL) {
		g_main_loop_unref(loop);
	}

	if (notify_is_initted()) {
		notify_uninit();
	}

	exit(status);
}
