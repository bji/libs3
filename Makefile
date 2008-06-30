
LIBS3_VER_MAJOR := 0
LIBS3_VER_MINOR := 1

ifndef CURL_LIBS
    CURL_LIBS := $(shell curl-config --libs)
endif

ifndef CURL_CFLAGS
    CURL_CFLAGS := $(shell curl-config --cflags)
endif

CFLAGS += -Wall -Werror -std=c99 -Iinc $(CURL_CFLAGS) \
          -DLIBS3_VER_MAJOR=$(LIBS3_VER_MAJOR) \
          -DLIBS3_VER_MINOR=$(LIBS3_VER_MINOR)

vpath .c src

all: libs3

libs3: lib/libs3.a

lib/libs3.a: src/acl.o src/auth_header.o src/bucket.o src/general.o \
             src/object.o src/request.o src/request_context.o src/service.o
	$(AR) cr $@ $^

test: bin/test

bin/test: src/test.o lib/libs3.a
	$(CC) -o $@ $^ -lpthread -lcurl -lssl

.PHONY: clean
clean:
	rm -f src/*.o lib/libs3.a bin/test
