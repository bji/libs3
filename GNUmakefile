# GNUmakefile
# 
# Copyright 2008 Bryan Ischo <bryan@ischo.com>
# 
# This file is part of libs3.
# 
# libs3 is free software: you can redistribute it and/or modify it under the
# terms of the GNU Lesser General Public License as published by the Free
# Software Foundation, version 3 of the License.
#
# In addition, as a special exception, the copyright holders give
# permission to link the code of this library and its programs with the
# OpenSSL library, and distribute linked combinations including the two.
#
# libs3 is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU Lesser General Public License
# version 3 along with libs3, in a file named COPYING.  If not, see
# <http://www.gnu.org/licenses/>.

# I tried to use the autoconf/automake/autolocal/etc (i.e. autohell) tools
# but I just couldn't stomach them.  Since this is a Makefile for POSIX
# systems, I will simply do away with autohell completely and use a GNU
# Makefile.  GNU make ought to be available pretty much everywhere, so I
# don't see this being a significant issue for portability.

# All commands assume a GNU compiler.  For systems which do not use a GNU
# compiler, write scripts with the same names as these commands, and taking
# the same arguments, and translate the arguments and commands into the
# appropriate non-POSIX ones as needed.  libs3 assumes a GNU toolchain as
# the most portable way to build software possible.  Non-POSIX, non-GNU
# systems can do the work of supporting this build infrastructure.


# --------------------------------------------------------------------------
# Set libs3 version number using git tags and optional developer PKG_PATCH
# environment variable
#

# use phony .FORCE to ensure this is always regenerated
.PHONY: .FORCE

GIT-VERSION-FILE: .FORCE
	./GIT-VERSION-GEN
-include GIT-VERSION-FILE

LIBS3_VER_MAJOR ?= $(wordlist 1,1, $(subst -, ,$(subst ., ,$(VERSION))))
LIBS3_VER_MINOR ?= $(wordlist 2,2, $(subst -, ,$(subst ., ,$(VERSION))))
LIBS3_VER := $(LIBS3_VER_MAJOR).$(LIBS3_VER_MINOR)


# -----------------------------------------------------------------------------
# Determine verbosity.  VERBOSE_SHOW should be prepended to every command which
# should only be displayed if VERBOSE is set.  QUIET_ECHO may be used to
# echo text only if VERBOSE is not set.  Typically, a VERBOSE_SHOW command will
# be paired with a QUIET_ECHO command, to provide a command which is displayed
# in VERBOSE mode, along with text which is displayed in non-VERBOSE mode to
# describe the command.
#
# No matter what VERBOSE is defined to, it ends up as true if it's defined.
# This will be weird if you defined VERBOSE=false in the environment, and we
# switch it to true here; but the meaning of VERBOSE is, "if it's defined to
# any value, then verbosity is turned on".  So don't define VERBOSE if you
# don't want verbosity in the build process.
# -----------------------------------------------------------------------------

ifdef VERBOSE
        VERBOSE = true
        VERBOSE_ECHO = @ echo
        VERBOSE_SHOW =
        QUIET_ECHO = @ echo > /dev/null
else
        VERBOSE = false
        VERBOSE_ECHO = @ echo > /dev/null
        VERBOSE_SHOW = @
        QUIET_ECHO = @ echo
endif


# --------------------------------------------------------------------------
# BUILD directory
ifndef BUILD
    ifdef DEBUG
        BUILD := build-debug
    else
        BUILD := build
    endif
endif

$(BUILD):
	$(VERBOSE_SHOW) mkdir -p $(BUILD)

# --------------------------------------------------------------------------
# DESTDIR directory
ifndef DESTDIR
    DESTDIR := /usr
endif

# --------------------------------------------------------------------------
# LIBDIR directory
ifndef LIBDIR
    LIBDIR := ${DESTDIR}/lib
endif

# --------------------------------------------------------------------------
# Compiler CC handling
ifndef CC
    CC := gcc
endif

# --------------------------------------------------------------------------
# Acquire configuration information for libraries that libs3 depends upon

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


# --------------------------------------------------------------------------
# These CFLAGS assume a GNU compiler.  For other compilers, write a script
# which converts these arguments into their equivalent for that particular
# compiler.

ifndef CFLAGS
    ifdef DEBUG
        CFLAGS := -g
    else
        CFLAGS := -O3
    endif
endif

CFLAGS += -Wall -Werror -Wshadow -Wextra -Iinc \
          $(CURL_CFLAGS) $(LIBXML2_CFLAGS) \
          -DLIBS3_VER_MAJOR=\"$(LIBS3_VER_MAJOR)\" \
          -DLIBS3_VER_MINOR=\"$(LIBS3_VER_MINOR)\" \
          -DLIBS3_VER=\"$(LIBS3_VER)\" \
          -D__STRICT_ANSI__ \
          -D_ISOC99_SOURCE \
          -D_POSIX_C_SOURCE=200112L

LDFLAGS = $(CURL_LIBS) $(LIBXML2_LIBS) -lpthread


# --------------------------------------------------------------------------
# Default targets are everything

.PHONY: all
all: exported test


# --------------------------------------------------------------------------
# Exported targets are the library and driver program

.PHONY: exported
exported: libs3 s3 headers


# --------------------------------------------------------------------------
# Install target

.PHONY: install
install: exported
	$(QUIET_ECHO) $(DESTDIR)/bin/s3: Installing executable
	$(VERBOSE_SHOW) install -Dps -m u+rwx,go+rx $(BUILD)/bin/s3 \
                    $(DESTDIR)/bin/s3
	$(QUIET_ECHO) \
        $(LIBDIR)/libs3.so.$(LIBS3_VER): Installing shared library
	$(VERBOSE_SHOW) install -Dps -m u+rw,go+r \
               $(BUILD)/lib/libs3.so.$(LIBS3_VER_MAJOR) \
               $(LIBDIR)/libs3.so.$(LIBS3_VER)
	$(QUIET_ECHO) \
        $(LIBDIR)/libs3.so.$(LIBS3_VER_MAJOR): Linking shared library
	$(VERBOSE_SHOW) ln -sf libs3.so.$(LIBS3_VER) \
               $(LIBDIR)/libs3.so.$(LIBS3_VER_MAJOR)
	$(QUIET_ECHO) $(LIBDIR)/libs3.so: Linking shared library
	$(VERBOSE_SHOW) ln -sf libs3.so.$(LIBS3_VER_MAJOR) $(LIBDIR)/libs3.so
	$(QUIET_ECHO) $(LIBDIR)/libs3.a: Installing static library
	$(VERBOSE_SHOW) install -Dp -m u+rw,go+r $(BUILD)/lib/libs3.a \
                    $(LIBDIR)/libs3.a
	$(QUIET_ECHO) $(DESTDIR)/include/libs3.h: Installing header
	$(VERBOSE_SHOW) install -Dp -m u+rw,go+r $(BUILD)/include/libs3.h \
                    $(DESTDIR)/include/libs3.h


# --------------------------------------------------------------------------
# Uninstall target

.PHONY: uninstall
uninstall:
	$(QUIET_ECHO) Installed files: Uninstalling
	$(VERBOSE_SHOW) \
	    rm -f $(DESTDIR)/bin/s3 \
              $(DESTDIR)/include/libs3.h \
              $(DESTDIR)/lib/libs3.a \
              $(DESTDIR)/lib/libs3.so \
              $(DESTDIR)/lib/libs3.so.$(LIBS3_VER_MAJOR) \
              $(DESTDIR)/lib/libs3.so.$(LIBS3_VER)


# --------------------------------------------------------------------------
# Compile target patterns

$(BUILD)/obj/%.o: src/%.c
	$(QUIET_ECHO) $@: Compiling object
	@ mkdir -p $(dir $(BUILD)/dep/$<)
	@ $(CC) $(CFLAGS) -M -MG -MQ $@ -DCOMPILINGDEPENDENCIES \
        -o $(BUILD)/dep/$(<:%.c=%.d) -c $<
	@ mkdir -p $(dir $@)
	$(VERBOSE_SHOW) $(CC) $(CFLAGS) -o $@ -c $<

$(BUILD)/obj/%.do: src/%.c
	$(QUIET_ECHO) $@: Compiling dynamic object
	@ mkdir -p $(dir $(BUILD)/dep/$<)
	@ $(CC) $(CFLAGS) -M -MG -MQ $@ -DCOMPILINGDEPENDENCIES \
        -o $(BUILD)/dep/$(<:%.c=%.dd) -c $<
	@ mkdir -p $(dir $@)
	$(VERBOSE_SHOW) $(CC) $(CFLAGS) -fpic -fPIC -o $@ -c $< 


# --------------------------------------------------------------------------
# libs3 library targets

LIBS3_SHARED = $(BUILD)/lib/libs3.so.$(LIBS3_VER_MAJOR)
LIBS3_STATIC = $(BUILD)/lib/libs3.a

.PHONY: libs3
libs3: $(LIBS3_SHARED) $(LIBS3_STATIC)

LIBS3_SOURCES := acl.c bucket.c error_parser.c general.c \
                 object.c request.c request_context.c \
                 response_headers_handler.c service_access_logging.c \
                 service.c simplexml.c util.c multipart.c

$(LIBS3_SHARED): $(LIBS3_SOURCES:%.c=$(BUILD)/obj/%.do)
	$(QUIET_ECHO) $@: Building shared library
	@ mkdir -p $(dir $@)
	$(VERBOSE_SHOW) $(CC) -shared -Wl,-soname,libs3.so.$(LIBS3_VER_MAJOR) \
        -o $@ $^ $(LDFLAGS)

$(LIBS3_STATIC): $(LIBS3_SOURCES:%.c=$(BUILD)/obj/%.o)
	$(QUIET_ECHO) $@: Building static library
	@ mkdir -p $(dir $@)
	$(VERBOSE_SHOW) $(AR) cr $@ $^


# --------------------------------------------------------------------------
# Driver program targets

.PHONY: s3
s3: $(BUILD)/bin/s3

$(BUILD)/bin/s3: $(BUILD)/obj/s3.o $(LIBS3_SHARED)
	$(QUIET_ECHO) $@: Building executable
	@ mkdir -p $(dir $@)
	$(VERBOSE_SHOW) $(CC) -o $@ $^ $(LDFLAGS)


# --------------------------------------------------------------------------
# libs3 header targets

.PHONY: headers
headers: $(BUILD)/include/libs3.h

$(BUILD)/include/libs3.h: inc/libs3.h
	$(QUIET_ECHO) $@: Linking header
	@ mkdir -p $(dir $@)
	$(VERBOSE_SHOW) ln -sf $(abspath $<) $@


# --------------------------------------------------------------------------
# Test targets

.PHONY: test
test: $(BUILD)/bin/testsimplexml

$(BUILD)/bin/testsimplexml: $(BUILD)/obj/testsimplexml.o $(LIBS3_STATIC)
	$(QUIET_ECHO) $@: Building executable
	@ mkdir -p $(dir $@)
	$(VERBOSE_SHOW) $(CC) -o $@ $^ $(LIBXML2_LIBS)


# --------------------------------------------------------------------------
# Clean target

.PHONY: clean
clean:
	$(QUIET_ECHO) $(BUILD): Cleaning
	$(VERBOSE_SHOW) rm -rf $(BUILD)

.PHONY: distclean
distclean:
	$(QUIET_ECHO) $(BUILD): Cleaning
	$(VERBOSE_SHOW) rm -rf $(BUILD)


# --------------------------------------------------------------------------
# Clean dependencies target

.PHONY: cleandeps
cleandeps:
	$(QUIET_ECHO) $(BUILD)/dep: Cleaning dependencies
	$(VERBOSE_SHOW) rm -rf $(BUILD)/dep


# --------------------------------------------------------------------------
# Dependencies

ALL_SOURCES := $(LIBS3_SOURCES) s3.c testsimplexml.c

$(foreach i, $(ALL_SOURCES), $(eval -include $(BUILD)/dep/src/$(i:%.c=%.d)))
$(foreach i, $(ALL_SOURCES), $(eval -include $(BUILD)/dep/src/$(i:%.c=%.dd)))

# --------------------------------------------------------------------------
# tar package target
#
GIT_TARNAME = libs3-$(FULL_VERSION)
GIT_TARPATH = $(BUILD)/$(GIT_TARNAME)

dist: $(BUILD) libs3.spec
	$(VERBOSE_SHOW) git archive --format=tar --prefix=$(GIT_TARNAME)/ HEAD^{tree} > $(GIT_TARPATH).tar
	$(VERBOSE_SHOW) mkdir -p $(GIT_TARNAME)
	$(VERBOSE_SHOW) cp GIT-VERSION-FILE $(GIT_TARNAME)
	$(VERBOSE_SHOW) cp $(BUILD)/libs3.spec $(GIT_TARNAME)
	$(VERBOSE_SHOW) tar -rf $(GIT_TARPATH).tar $(GIT_TARNAME)/GIT-VERSION-FILE $(GIT_TARNAME)/libs3.spec
	$(VERBOSE_SHOW) rm -r $(GIT_TARNAME)
	$(VERBOSE_SHOW) gzip -f -9 $(GIT_TARPATH).tar

# --------------------------------------------------------------------------
# Redhat RPM target

# RPM releases can't have -
RPM_RELEASE := $(shell echo $(RELEASE) | tr '-' '.')

%.spec: %.spec.in .FORCE $(BUILD)
	$(VERBOSE_SHOW) sed -e 's/@@VERSION@@/$(VERSION)/g' \
		-e s'/@@TAR_NAME@@/$(GIT_TARNAME)/g' \
		-e s'/@@RELEASE@@/$(RPM_RELEASE)/g' < $< > $@+
	$(VERBOSE_SHOW) mv $@+ $(BUILD)/$@

# --------------------------------------------------------------------------
# Debian package target

DEBPKG = $(BUILD)/pkg/libs3_$(LIBS3_VER).deb
DEBDEVPKG = $(BUILD)/pkg/libs3-dev_$(LIBS3_VER).deb

.PHONY: deb
deb: $(DEBPKG) $(DEBDEVPKG)

$(DEBPKG): DEBARCH = $(shell dpkg-architecture | grep ^DEB_BUILD_ARCH= | \
                       cut -d '=' -f 2)
$(DEBPKG): exported $(BUILD)/deb/DEBIAN/control $(BUILD)/deb/DEBIAN/shlibs \
           $(BUILD)/deb/DEBIAN/postinst \
           $(BUILD)/deb/usr/share/doc/libs3/changelog.gz \
           $(BUILD)/deb/usr/share/doc/libs3/changelog.Debian.gz \
           $(BUILD)/deb/usr/share/doc/libs3/copyright
	DESTDIR=$(BUILD)/deb/usr $(MAKE) install
	rm -rf $(BUILD)/deb/usr/include
	rm -f $(BUILD)/deb/usr/lib/libs3.a
	@mkdir -p $(dir $@)
	fakeroot dpkg-deb -b $(BUILD)/deb $@
	mv $@ $(BUILD)/pkg/libs3_$(LIBS3_VER)_$(DEBARCH).deb

$(DEBDEVPKG): DEBARCH = $(shell dpkg-architecture | grep ^DEB_BUILD_ARCH= | \
                          cut -d '=' -f 2)
$(DEBDEVPKG): exported $(BUILD)/deb-dev/DEBIAN/control \
           $(BUILD)/deb-dev/usr/share/doc/libs3-dev/changelog.gz \
           $(BUILD)/deb-dev/usr/share/doc/libs3-dev/changelog.Debian.gz \
           $(BUILD)/deb-dev/usr/share/doc/libs3-dev/copyright
	DESTDIR=$(BUILD)/deb-dev/usr $(MAKE) install
	rm -rf $(BUILD)/deb-dev/usr/bin
	rm -f $(BUILD)/deb-dev/usr/lib/libs3.so*
	@mkdir -p $(dir $@)
	fakeroot dpkg-deb -b $(BUILD)/deb-dev $@
	mv $@ $(BUILD)/pkg/libs3-dev_$(LIBS3_VER)_$(DEBARCH).deb

$(BUILD)/deb/DEBIAN/control: debian/control
	@mkdir -p $(dir $@)
	echo -n "Depends: " > $@
	dpkg-shlibdeps -Sbuild -O $(BUILD)/lib/libs3.so.$(LIBS3_VER_MAJOR) | \
            cut -d '=' -f 2- >> $@
	sed -e 's/LIBS3_VERSION/$(LIBS3_VER)/' \
            < $< | sed -e 's/DEBIAN_ARCHITECTURE/$(DEBARCH)/' | \
            grep -v ^Source: >> $@

$(BUILD)/deb-dev/DEBIAN/control: debian/control.dev
	@mkdir -p $(dir $@)
	sed -e 's/LIBS3_VERSION/$(LIBS3_VER)/' \
            < $< | sed -e 's/DEBIAN_ARCHITECTURE/$(DEBARCH)/' > $@

$(BUILD)/deb/DEBIAN/shlibs:
	echo -n "libs3 $(LIBS3_VER_MAJOR) libs3 " > $@
	echo "(>= $(LIBS3_VER))" >> $@

$(BUILD)/deb/DEBIAN/postinst: debian/postinst
	@mkdir -p $(dir $@)
	cp $< $@

$(BUILD)/deb/usr/share/doc/libs3/copyright: LICENSE
	@mkdir -p $(dir $@)
	cp $< $@
	@echo >> $@
	@echo -n "An alternate location for the GNU General Public " >> $@
	@echo "License version 3 on Debian" >> $@
	@echo "systems is /usr/share/common-licenses/GPL-3." >> $@

$(BUILD)/deb-dev/usr/share/doc/libs3-dev/copyright: LICENSE
	@mkdir -p $(dir $@)
	cp $< $@
	@echo >> $@
	@echo -n "An alternate location for the GNU General Public " >> $@
	@echo "License version 3 on Debian" >> $@
	@echo "systems is /usr/share/common-licenses/GPL-3." >> $@

$(BUILD)/deb/usr/share/doc/libs3/changelog.gz: debian/changelog
	@mkdir -p $(dir $@)
	gzip --best -c $< > $@

$(BUILD)/deb-dev/usr/share/doc/libs3-dev/changelog.gz: debian/changelog
	@mkdir -p $(dir $@)
	gzip --best -c $< > $@

$(BUILD)/deb/usr/share/doc/libs3/changelog.Debian.gz: debian/changelog.Debian
	@mkdir -p $(dir $@)
	gzip --best -c $< > $@

$(BUILD)/deb-dev/usr/share/doc/libs3-dev/changelog.Debian.gz: \
    debian/changelog.Debian
	@mkdir -p $(dir $@)
	gzip --best -c $< > $@


