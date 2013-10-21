## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2007 Peng Wu
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

AUTOMAKE_OPTIONS 	= gnu
SUBDIRS 		= include storage lookup

EXTRA_DIST		= libzhuyin.ver

MAINTAINERCLEANFILES 	= Makefile.in 

CLEANFILES		= *.bak 

ACLOCAL			= aclocal -I $(ac_aux_dir)

INCLUDES                = -I$(top_srcdir)/src \
			  -I$(top_srcdir)/src/include \
			  -I$(top_srcdir)/src/storage \
			  -I$(top_srcdir)/src/lookup \
                          @GLIB2_CFLAGS@

libzhuyinincludedir	= $(includedir)/libzhuyin-@VERSION@

libzhuyininclude_HEADERS= zhuyin.h

noinst_HEADERS		= zhuyin_internal.h

lib_LTLIBRARIES		= libzhuyin.la

noinst_LTLIBRARIES	= libzhuyin_internal.la

libzhuyin_la_SOURCES	= zhuyin.cpp

libzhuyin_la_LIBADD	= storage/libstorage.la lookup/liblookup.la @GLIB2_LIBS@

libzhuyin_la_LDFLAGS	= -Wl,--version-script=$(srcdir)/libzhuyin.ver \
			  -version-info @LT_VERSION_INFO@

libzhuyin_internal_la_SOURCES	= zhuyin_internal.cpp

libzhuyin_internal_la_LIBADD	= storage/libstorage.la lookup/liblookup.la


## Note:
## As libzhuyin internal interface will change, only provides static library
##   to catch errors when compiling instead of running.
