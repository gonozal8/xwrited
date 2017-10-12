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

#include <glib.h>
#ifdef	HAVE_GLIB_GDBUS
#include <gio/gio.h>
#else
#include <dbus/dbus-glib.h>
#endif /* HAVE_GLIB_GDBUS */
#include <dbus/dbus.h>

#include "xwrited-unique.h"

G_DEFINE_TYPE(XWritedUnique, xwrited_unique, G_TYPE_OBJECT)

#define	XWRITED_UNIQUE_GET_PRIVATE(obj)	(G_TYPE_INSTANCE_GET_PRIVATE((obj), \
    XWRITED_TYPE_UNIQUE, XWritedUniquePrivate))

struct	_XWritedUniquePrivate {
#ifdef HAVE_GLIB_GDBUS
	GDBusProxy	*session_bus_proxy;
#else
	DBusGConnection	*session_bus;
	DBusGProxy	*session_bus_proxy;
#endif /* HAVE_GLIB_GDBUS */
	gchar		*name;
	gboolean	is_unique;
};

enum {
	PROP_0,
	PROP_NAME,
	PROP_IS_XWRITED_UNIQUE
};

static gboolean
request_name(XWritedUnique *self)
{
	guint32	request_name_response;
	GError	*error = NULL;
#ifdef HAVE_GLIB_GDBUS
	GVariant *result;

	g_return_val_if_fail(self->priv->session_bus_proxy != NULL, FALSE);

	result = g_dbus_proxy_call_sync(self->priv->session_bus_proxy,
	    "RequestName", g_variant_new("(su)", self->priv->name,
	    DBUS_NAME_FLAG_DO_NOT_QUEUE), G_DBUS_CALL_FLAGS_NONE, -1, NULL,
	    &error);
	if (result == NULL) {
		g_warning("failed to acquire service name \"%s\": %s",
		    self->priv->name, error->message);
		g_error_free(error);
		return (FALSE);
	}

	g_variant_get(result, "(u)", &request_name_response);
	g_variant_unref(result);
#else

	g_return_val_if_fail(self->priv->session_bus != NULL, FALSE);
	g_return_val_if_fail(self->priv->session_bus_proxy != NULL, FALSE);
	if (dbus_g_proxy_call(self->priv->session_bus_proxy, "RequestName",
	    &error, G_TYPE_STRING, self->priv->name, G_TYPE_UINT,
	    DBUS_NAME_FLAG_DO_NOT_QUEUE, G_TYPE_INVALID, G_TYPE_UINT,
	    &request_name_response, G_TYPE_INVALID) == 0) {
		g_warning("failed to acquire service name \"%s\": %s",
		    self->priv->name, error->message);
		g_error_free(error);
		return (FALSE);
	}
#endif /* HAVE_GLIB_GDBUS */

	switch (request_name_response) {
	case DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER:
		return (TRUE);
	case DBUS_REQUEST_NAME_REPLY_EXISTS:
		break;
	default:
		g_warning("failed to acquire service name \"%s\"",
		    self->priv->name);
	}

	return (FALSE);
}

static void
xwrited_unique_get_property(GObject *gobject, guint property_id, GValue *value,
    GParamSpec *pspec)
{
	XWritedUnique	*app = XWRITED_UNIQUE(gobject);

	switch (property_id) {
	case PROP_NAME:
		g_value_set_string(value, app->priv->name);
		break;
	case PROP_IS_XWRITED_UNIQUE:
		g_value_set_boolean(value, app->priv->is_unique);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, property_id,
		    pspec);
	}
}

static void
xwrited_unique_set_property(GObject *gobject, guint property_id,
    const GValue *value, GParamSpec *pspec)
{
	XWritedUnique	*app = XWRITED_UNIQUE(gobject);

	switch (property_id) {
	case PROP_NAME:
		app->priv->name = g_strdup(g_value_get_string(value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, property_id,
		    pspec);
	}
}

static GObject *
xwrited_unique_constructor(GType gtype, guint n_params,
    GObjectConstructParam *params)
{
	GObjectClass	*parent_class;
	GObject		*gobject;
	XWritedUnique	*app;
	GError		*error = NULL;

	parent_class = G_OBJECT_CLASS(xwrited_unique_parent_class);
	gobject = parent_class->constructor(gtype, n_params, params);
	app = XWRITED_UNIQUE(gobject);

#ifdef HAVE_GLIB_GDBUS
	app->priv->session_bus_proxy =
	    g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
		G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
		G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START, NULL, DBUS_SERVICE_DBUS,
		DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS, NULL, &error);
	if (app->priv->session_bus_proxy == NULL) {
		g_warning("failed to create DBus proxy: %s", error->message);
		g_error_free(error);
		goto out;
	}
#else
	app->priv->session_bus = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
	if (app->priv->session_bus == NULL) {
		g_warning("failed to connect to DBus session bus: %s",
		    error->message);
		g_error_free(error);
		goto out;
	}

	app->priv->session_bus_proxy =
	    dbus_g_proxy_new_for_name(app->priv->session_bus,
	    DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);
	if (app->priv->session_bus_proxy == NULL) {
		g_warning("failed to create DBus proxy");
		goto out;
	}
#endif /* HAVE_GLIB_GDBUS */

	if (request_name(app)) {
		app->priv->is_unique = TRUE;
	}

out:
	return (gobject);
}

static void
xwrited_unique_dispose(GObject *gobject)
{
	XWritedUnique	*self = XWRITED_UNIQUE(gobject);

	if (self->priv->session_bus_proxy != NULL) {
		g_object_unref(self->priv->session_bus_proxy);
		self->priv->session_bus_proxy = NULL;
	}

#ifndef HAVE_GLIB_GDBUS
	if (self->priv->session_bus != NULL) {
		dbus_g_connection_unref(self->priv->session_bus);
		self->priv->session_bus = NULL;
	}

#endif /* !HAVE_GLIB_GDBUS */
	G_OBJECT_CLASS(xwrited_unique_parent_class)->dispose(gobject);
}

static void
xwrited_unique_finalize(GObject *gobject)
{
	XWritedUnique	*self = XWRITED_UNIQUE(gobject);

	g_free(self->priv->name);

	G_OBJECT_CLASS(xwrited_unique_parent_class)->finalize(gobject);
}

static void
xwrited_unique_class_init(XWritedUniqueClass *klass)
{
	GObjectClass	*gobject_class = G_OBJECT_CLASS(klass);
	GParamSpec	*pspec;

	gobject_class->constructor = xwrited_unique_constructor;
	gobject_class->get_property = xwrited_unique_get_property;
	gobject_class->set_property = xwrited_unique_set_property;
	gobject_class->dispose = xwrited_unique_dispose;
	gobject_class->finalize = xwrited_unique_finalize;

	pspec = g_param_spec_string("name", "Name",
	    "The unique name of the application", NULL, G_PARAM_READABLE |
	    G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME |
	    G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);
	g_object_class_install_property(gobject_class, PROP_NAME, pspec);

	pspec = g_param_spec_boolean("is-unique", "Is unique",
	    "Whether the current application instance is unique", FALSE,
	    G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
	    G_PARAM_STATIC_BLURB);
	g_object_class_install_property(gobject_class, PROP_IS_XWRITED_UNIQUE,
	    pspec);

	g_type_class_add_private(klass, sizeof (XWritedUniquePrivate));
}

static void
xwrited_unique_init(XWritedUnique *self)
{
	self->priv = XWRITED_UNIQUE_GET_PRIVATE(self);

	self->priv->is_unique = FALSE;
#ifndef HAVE_GLIB_GDBUS
	self->priv->session_bus = NULL;
#endif /* !HAVE_GLIB_GDBUS */
	self->priv->session_bus_proxy = NULL;
}

XWritedUnique *
xwrited_unique_new(const gchar *name)
{
	XWritedUnique	*app;

	g_return_val_if_fail(name != NULL, NULL);

	app = g_object_new(XWRITED_TYPE_UNIQUE, "name", name, NULL);
	if (
#ifndef HAVE_GLIB_GDBUS
	    app->priv->session_bus == NULL ||
#endif /* !HAVE_GLIB_GDBUS */
	    app->priv->session_bus_proxy == NULL) {
		g_object_unref(app);
		return (NULL);
	}

	return (app);
}

gboolean
xwrited_unique_is_unique(XWritedUnique *self)
{
	g_return_val_if_fail(XWRITED_IS_UNIQUE(self), FALSE);

	return (self->priv->is_unique);
}
