# Makefile
# 
# Copyright 2008 Bryan Ischo <bryan@ischo.com>
# 
# This file is part of libs3.
# 
# libs3 is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, version 3 of the License.
#
# libs3 is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License version 3
# along with libs3, in a file named COPYING.  If not, see
# <http://www.gnu.org/licenses/>.


LIBS3_VER_MAJOR := 0
LIBS3_VER_MINOR := 2

ifndef CURL_LIBS
    CURL_LIBS := $(shell curl-config --libs)
endif

ifndef CURL_CFLAGS
    CURL_CFLAGS := $(shell curl-config --cflags)
endif

ifndef LIBXML2_LIBS
    LIBXML2_LIBS := $(shell xml2-config --libs)
endif

ifndef LIBXML2_CFLAGS
    LIBXML2_CFLAGS := $(shell xml2-config --cflags)
endif

CFLAGS += -Wall -Werror -std=c99 -Iinc $(CURL_CFLAGS) $(LIBXML2_CFLAGS) \
          -DLIBS3_VER_MAJOR=$(LIBS3_VER_MAJOR) \
          -DLIBS3_VER_MINOR=$(LIBS3_VER_MINOR)

all: libs3 s3

libs3: lib/libs3.a

lib/libs3.a: src/acl.o src/bucket.o src/error_parser.o src/general.o \
             src/object.o src/request.o src/request_context.o \
             src/response_headers_handler.o src/service.o src/simplexml.o \
             src/util.o
	$(AR) cr $@ $^

s3: bin/s3

bin/s3: src/s3.o lib/libs3.a
	$(CC) -o $@ $^ $(CURL_LIBS) $(LIBXML2_LIBS) -lpthread -lssl

test: bin/testsimplexml

bin/testsimplexml: src/testsimplexml.o lib/libs3.a
	$(CC) -o $@ $^ $(LIBXML2_LIBS)

.PHONY: clean
clean:
	rm -f src/*.o lib/libs3.a bin/s3
