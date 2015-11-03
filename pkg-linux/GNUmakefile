#
#    VSM_notice_begin
#
#	VSM - Versity Storage Management File System
#
#	Copyright (c) 2015  Versity Software, Inc.
#	All Rights Reserved
#
#    VSM_notice_end
#
#
#       LGPL NOTICE
#
#       This library is free software; you can redistribute it and/or
#       modify it under the terms of the GNU Lesser General Public
#       License as published by the Free Software Foundation; either
#       version 2.1 of the License, or (at your option) any later version.
#
#       This library is distributed in the hope that it will be useful,
#       but WITHOUT ANY WARRANTY; without even the implied warranty of
#       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#       Lesser General Public License for more details.
#
#       You should have received a copy of the GNU Lesser General Public
#       License along with this library; if not, write to the Free Software
#       Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA


DATETIME = $(shell date '+%Y-%m-%d %H:%M:%S')
MARKER = $(DEPTH)/MARKER

RPM_DIR = $(HOME)/rpmbuild

# fix up source name to be proper and polite
ifneq ("$(VSM_PATCH)", "")
    PKG_SOURCE_VERSION = $(PKG_VERSION)-$(VSM_PATCH)
    RPM_NAME = $(PKG_VERSION)*$(VSM_PATCH).*.rpm
else
    PKG_SOURCE_VERSION = $(PKG_VERSION)
    RPM_NAME = $(PKG_VERSION).*.rpm
endif

.PHONY: dist devdist
dist:
	@echo "## Build libs3 tar for libs3-$(PKG_SOURCE_VERSION)"
	rm -fv $(RPM_DIR)/SOURCES/libs3-$(PKG_SOURCE_VERSION).tar.gz

	test -d ../.git && \
		(cd ..; git archive --prefix libs3-$(PKG_SOURCE_VERSION)/ \
				  -o $(RPM_DIR)/SOURCES/libs3-$(PKG_SOURCE_VERSION).tar HEAD) || /bin/true
	pigz $(RPM_DIR)/SOURCES/libs3-$(PKG_SOURCE_VERSION).tar

devdist: dist
	@echo "## Generating diff for developer patch"
	test -d ../.git && \
		(cd ..; git diff -U -p --stat > $(RPM_DIR)/SOURCES/libs3-dev-$(PKG_SOURCE_VERSION)-$(USER).diff) || /bin/true

rpm:
	@echo "## Build RPM for $(PKG_SOURCE_VERSION)"
	env PKG_VERSION=$(PKG_VERSION) bash ./mock_rpmbuild.sh

devrpm:
	@echo "## Building Developer RPM for $(PKG_SOURCE_VERSION)"
	env PKG_VERSION=$(PKG_VERSION) bash ./mock_rpmbuild.sh

__dummy := $(shell [ -d $(OBJ_DIR) ] || mkdir -p $(OBJ_DIR) )

