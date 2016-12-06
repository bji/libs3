ACLOCAL_AMFLAGS = -I m4
AM_CPPFLAGS = ${curl_CFLAGS} ${xml_CFLAGS} -Iinc -DLIBS3_VER_MAJOR=\"2\" -DLIBS3_VER_MINOR=\"0\" -DLIBS3_VER=\"2.0\" -D_POSIX_C_SOURCE=200112L
AM_CFLAGS = -Wall -Wshadow -Wextra
bin_PROGRAMS = s3
noinst_PROGRAMS = testsimplexml
lib_LTLIBRARIES = libs3.la
s3_SOURCES = src/s3.c
s3_LDADD = libs3.la
libs3_la_SOURCES = src/acl.c src/bucket.c src/error_parser.c src/general.c src/object.c src/request.c src/request_context.c src/response_headers_handler.c src/service_access_logging.c src/service.c src/simplexml.c src/util.c src/multipart.c
libs3_la_LDFLAGS = -version-number 2:0:0 -export-symbols-regex '^(S3|simplexml)_'
libs3_la_LIBADD = -lpthread ${curl_LIBS} ${xml_LIBS}
include_HEADERS = inc/libs3.h
testsimplexml_SOURCES = src/testsimplexml.c
testsimplexml_LDADD = libs3.la
