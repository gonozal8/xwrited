#
# Copyright (C) 2016 Guido Berhoerster <guido+xwrited@berhoerster.name>
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

PACKAGE =	xwrited
APP_NAME =	org.guido-berhoerster.code.xwrited
VERSION =	2
DISTNAME :=	$(PACKAGE)-$(VERSION)

# gcc, clang, icc, Sun/Solaris Studio
CC :=		$(CC) -std=c99
COMPILE.c =	$(CC) $(CFLAGS) $(XCFLAGS) $(CPPFLAGS) $(XCPPFLAGS) $(TARGET_ARCH) -c
# gcc, clang, icc
MAKEDEPEND.c =	$(CC) -MM $(CFLAGS) $(XCFLAGS) $(CPPFLAGS) $(XCPPFLAGS)
# Sun/Solaris Studio
#MAKEDEPEND.c =	$(CC) -xM1 $(CFLAGS) $(XCFLAGS) $(CPPFLAGS) $(XCPPFLAGS)
# X makedepend
#MAKEDEPEND.c =	makedepend -f- -Y -- $(CFLAGS) $(XCFLAGS) $(CPPFLAGS) $(XCPPFLAGS) --
INSTALL :=	install
INSTALL.exec :=	$(INSTALL) -D -m 0755
INSTALL.data :=	$(INSTALL) -D -m 0644
PAX :=		pax
GZIP :=		gzip
SED :=		sed
PASTE :=	paste
MSGFMT :=	msgfmt
INTLTOOL_UPDATE := intltool-update
INTLTOOL_MERGE := intltool-merge
XSLTPROC :=	xsltproc
DOCBOOK5_MANPAGES_STYLESHEET =	http://docbook.sourceforge.net/release/xsl-ns/current/manpages/docbook.xsl

define generate-manpage-rule =
%.$(1): %.$(1).xml
	$$(XSLTPROC) \
	    --xinclude \
	    --stringparam package $$(PACKAGE) \
	    --stringparam version $$(VERSION)\
	    data/docbook-update-source-data.xsl $$< | \
	    $$(XSLTPROC) \
	    --xinclude \
	    $$(DOCBOOK5_MANPAGES_FLAGS) \
	    --output $$@ \
	    $$(DOCBOOK5_MANPAGES_STYLESHEET) \
	    -
endef

DESTDIR ?=
prefix ?=	/usr/local
bindir ?=	$(prefix)/bin
datadir ?=	$(prefix)/share
mandir ?=	$(datadir)/man
localedir ?=	$(datadir)/locale
sysconfdir ?=	/etc
xdgautostartdir ?= $(sysconfdir)/xdg/autostart

OS_NAME :=	$(shell uname -s)
OS_RELEASE :=	$(shell uname -r)

ifeq ($(shell pkg-config --exists 'glib-2.0 >= 2.25.5' && printf "true"),true)
  HAVE_GLIB_GDBUS = 1
else
  HAVE_GLIB_GDBUS = 0
endif

PKGCONFIG_LIBS := dbus-1 glib-2.0 libnotify
ifeq ($(HAVE_GLIB_GDBUS),1)
  PKGCONFIG_LIBS +=	dbus-glib-1
endif
OBJS =		main.o xwrited-debug.o xwrited-unique.o
ifneq ($(findstring $(OS_NAME),Linux FreeBSD),)
  OBJS +=	xwrited-utmp-utempter.o
else
  OBJS +=	xwrited-utmp-utmpx.o
endif
MANPAGES =	data/$(PACKAGE).1
AUTOSTART_FILE = data/$(PACKAGE).desktop
MOFILES :=	$(patsubst %.po,%.mo,$(wildcard po/*.po))
POTFILE =	po/$(PACKAGE).pot
POSRCS :=	$(shell $(SED) -e 's/\#.*//' -e '/^[ \t]*$$/d' \
		-e 's/^\[[^]]*\]//' po/POTFILES.in | $(PASTE) -s -d ' ')
DOCBOOK5_MANPAGES_FLAGS = --stringparam man.authors.section.enabled 0 \
			  --stringparam man.copyright.section.enabled 0

.DEFAULT_TARGET = all

.PHONY: all clean clobber dist install

all: $(PACKAGE) $(MANPAGES) $(MOFILES) $(AUTOSTART_FILE)

$(PACKAGE): CPPFLAGS :=	$(shell pkg-config --cflags $(PKGCONFIG_LIBS)) \
			-D_XOPEN_SOURCE=600 \
			-DPACKAGE="\"$(PACKAGE)\"" \
			-DAPP_NAME=\"$(APP_NAME)\" \
			-DVERSION=\"$(VERSION)\" \
			-DLOCALEDIR="\"$(localedir)\"" \
			-DG_LOG_DOMAIN=\"$(PACKAGE)\"
$(PACKAGE): LDLIBS :=	$(shell pkg-config --libs $(PKGCONFIG_LIBS))
ifeq ($(HAVE_GLIB_GDBUS),1)
  $(PACKAGE): XCPPFLAGS += -DHAVE_GLIB_GDBUS
endif
ifeq ($(OS_NAME),Linux)
  $(PACKAGE): LDLIBS +=	-lutempter
else ifeq ($(OS_NAME),FreeBSD)
  $(PACKAGE): LDLIBS +=	-lulog
else ifeq ($(OS_NAME),SunOS)
  $(PACKAGE): XCPPFLAGS += -I/usr/xpg4/include
  $(PACKAGE): XLDFLAGS += -L/usr/xpg4/lib -R/usr/xpg4/lib
endif
$(PACKAGE): $(OBJS)
	$(LINK.o) $^ $(LDLIBS) -o $@

$(POTFILE): po/POTFILES.in $(POSRCS)
	cd po/ && $(INTLTOOL_UPDATE) --pot --gettext-package="$(PACKAGE)"

pot: $(POTFILE)

update-po: $(POTFILE)
	cd po/ && for lang in $(patsubst po/%.mo,%,$(MOFILES)); do \
	    $(INTLTOOL_UPDATE) --dist --gettext-package="$(PACKAGE)" \
	    $${lang}; \
	done

%.o: %.c
	$(MAKEDEPEND.c) $< | $(SED) -f deps.sed >$*.d
	$(COMPILE.c) -o $@ $<

$(foreach section,1 2 3 4 5 6 7 8 9,$(eval $(call generate-manpage-rule,$(section))))

%.desktop: %.desktop.in $(MOFILES)
	$(INTLTOOL_MERGE) --desktop-style --utf8 po $< $@

%.mo: %.po
	$(MSGFMT) -o $@ $<

install:
	$(INSTALL.exec) $(PACKAGE) "$(DESTDIR)$(bindir)/$(PACKAGE)"
	for lang in $(patsubst po/%.mo,%,$(MOFILES)); do \
	    $(INSTALL.data) po/$${lang}.mo \
	        "$(DESTDIR)$(localedir)/$${lang}/LC_MESSAGES/$(PACKAGE).mo"; \
	done
	$(INSTALL.data) $(AUTOSTART_FILE) \
	        "$(DESTDIR)$(xdgautostartdir)/$(notdir $(AUTOSTART_FILE))"
	$(INSTALL.data) data/$(PACKAGE).1 \
	        "$(DESTDIR)$(mandir)/man1/$(PACKAGE).1"

clean:
	rm -f $(PACKAGE) $(OBJS) $(POTFILE) $(MOFILES) $(MANPAGES) $(AUTOSTART_FILE)

clobber: clean
	rm -f $(patsubst %.o,%.d,$(OBJS))

dist: clobber
	$(PAX) -w -x ustar -s ',.*/\..*,,' -s ',./[^/]*\.tar\.gz,,' \
	    -s ',^\.$$,,' -s ',\./,$(DISTNAME)/,' . | \
	    $(GZIP) > $(DISTNAME).tar.gz

-include $(patsubst %.o,%.d,$(OBJS))
