/*
 * Copyright (C) 2011 Guido Berhoerster <guido+xwrited@berhoerster.name>
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

#ifndef	XWRITED_UNIQUE_H
#define	XWRITED_UNIQUE_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define	XWRITED_TYPE_UNIQUE		(xwrited_unique_get_type())
#define	XWRITED_UNIQUE(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), \
    XWRITED_TYPE_UNIQUE, XWritedUnique))
#define	XWRITED_IS_UNIQUE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    XWRITED_TYPE_UNIQUE))
#define	XWRITED_UNIQUE_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), \
    XWRITED_TYPE_UNIQUE, XWritedUniqueClass))
#define	XWRITED_IS_UNIQUE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), \
    XWRITED_TYPE_UNIQUE))
#define	XWRITED_UNIQUE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), \
    XWRITED_TYPE_UNIQUE, XWritedUniqueClass))

typedef struct _XWritedUnique		XWritedUnique;
typedef struct _XWritedUniqueClass	XWritedUniqueClass;
typedef struct _XWritedUniquePrivate	XWritedUniquePrivate;

struct _XWritedUnique {
	GObject		parent_instance;
	XWritedUniquePrivate *priv;
};

struct _XWritedUniqueClass {
	GObjectClass	parent_class;
};

GType		xwrited_unique_get_type(void) G_GNUC_CONST;
gboolean	xwrited_unique_is_unique(XWritedUnique *);
XWritedUnique *	xwrited_unique_new(const gchar *);

G_END_DECLS

#endif /* XWRITED_UNIQUE_H */
