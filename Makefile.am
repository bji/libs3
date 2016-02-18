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
        test/test.sh \
        test/testsimplexml.sh \
	test/badxml_01.xml \
	test/goodxml_01.xml \
	test/goodxml_02.xml \
	test/goodxml_03.xml
	

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

include_HEADERS = inc/libs3.h inc/rgadmin.h
lib_LTLIBRARIES = libglobus_s3.la
bin_PROGRAMS = s3

pkgconfdir = $(libdir)/pkgconfig
pkgconf_DATA = libglobus_s3.pc

libglobus_s3_la_SOURCES = \
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
        src/util.c \
        inc/libs3.h \
        inc/request.h \
        inc/simplexml.h \
        inc/rgadmin.h \
        src/rgadmin.c

if MINGW
libglobus_s3_la_SOURCES += \
        src/mingw_functions.c \
        src/mingw_s3_functions.c
endif

libglobus_s3_la_LDFLAGS = -no-undefined \
        -version-info 0:1:0 \
	$(AM_LDFLAGS)

s3_SOURCES = src/s3.c
s3_LDADD = libglobus_s3.la $(AM_LDFLAGS)

check_PROGRAMS = testsimplexml
testsimplexml_SOURCES = src/testsimplexml.c
testsimplexml_LDADD = libglobus_s3.la $(AM_LDFLAGS)

TESTS = test/test.sh test/testsimplexml.sh
TESTS_ENVIRONMENT = export PATH="$(abs_builddir):$${PATH}"; export top_srcdir="${top_srcdir}";

if DOXYGEN
doc: doc-stamp
doc-stamp: inc/libs3.h
	srcdir=$(srcdir)/ $(DOXYGEN) $(srcdir)/doxyfile
	touch doc-stamp
clean-local:
	rm -rf dox doc-stamp
endif
