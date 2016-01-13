ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	COPYING \
	ChangeLog \
	INSTALL \
	LICENSE \
	README \
	TODO \
	archlinux/PKGBUILD \
	debian/changelog \
	debian/changelog.Debian \
	debian/control \
	debian/control.dev \
	debian/postinst \
	doxyfile \
	libs3.spec \
	mswin/libs3.def \
	mswin/rmrf.bat \
        test/test.sh

AM_CPPFLAGS = -I$(srcdir)/inc \
          $(CURL_CFLAGS) $(XML_CPPFLAGS) \
          -DLIBS3_VER_MAJOR=\"$(LIBS3_VER_MAJOR)\" \
          -DLIBS3_VER_MINOR=\"$(LIBS3_VER_MINOR)\" \
          -DLIBS3_VER=\"$(LIBS3_VER)\" \
          -D_POSIX_C_SOURCE=200112L \
	  -D_XOPEN_SOURCE=600

AM_LDFLAGS = $(LIBCURL) $(XML_LIBS)

if !MINGW
AM_LDFLAGS += -lpthread
endif

include_HEADERS = inc/libs3.h
lib_LTLIBRARIES = libs3.la
bin_PROGRAMS = s3

pkgconfdir = $(libdir)/pkgconfig
pkgconf_DATA = libs3.pc

libs3_la_SOURCES = \
	inc/error_parser.h \
	inc/libs3.h \
	inc/request.h \
	inc/request_context.h \
	inc/response_headers_handler.h \
	inc/simplexml.h \
	inc/string_buffer.h \
	inc/util.h \
        src/acl.c \
        src/bucket.c \
        src/error_parser.c \
        src/general.c \
        src/multipart.c \
        src/object.c \
        src/request.c  \
        src/request_context.c \
        src/response_headers_handler.c \
        src/service.c \
        src/service_access_logging.c \
        src/simplexml.c \
        src/util.c
if MINGW
libs3_la_SOURCES += \
        src/mingw_functions.c \
        src/mingw_s3_functions.c
endif

libs3_la_LDFLAGS = -no-undefined \
        -version-info ${LIBS3_VER_MAJOR}:${LIBS3_VER_MINOR}:0 \
	$(AM_LDFLAGS)

s3_SOURCES = src/s3.c
s3_LDADD = libs3.la $(AM_LDFLAGS)

TESTS = test/test.sh

if DOXYGEN
doc: doc-stamp
doc-stamp: inc/libs3.h
	srcdir=$(srcdir)/ $(DOXYGEN) $(srcdir)/doxyfile
	touch doc-stamp
clean-local:
	rm -rf dox doc-stamp
endif
