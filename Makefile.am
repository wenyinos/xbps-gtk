ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = foreign subdir-objects
AM_CPPFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/src -I$(top_srcdir)/../include -I$(top_srcdir)/../include/xbps
AM_LDFLAGS = -L$(top_srcdir)/../lib -Wl,-rpath,$(top_srcdir)/../lib

bin_PROGRAMS = xbps-gtk

xbps_gtk_SOURCES = \
	src/main.c \
	src/callbacks.c \
	src/package.c \
	src/repository.c \
	src/transaction.c \
	src/window.c

xbps_gtk_CFLAGS = $(GTK_CFLAGS) $(AM_CPPFLAGS)
xbps_gtk_LDADD = $(GTK_LIBS) -lxbps -larchive -lz -lssl -lcrypto -lpthread

EXTRA_DIST = autogen.sh Makefile.am po/Makefile.in.in

distclean-local:
	rm -f Makefile config.status config.log

.PHONY: distclean-local