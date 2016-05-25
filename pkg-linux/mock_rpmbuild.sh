#!/bin/bash
#
#    VSM_notice_begin
#
# Copyright (c) 2015 Versity Software, Inc.
# All Rights Reserved.
#
# This file is protected by copyright. Copying, distributing or
# modifying the file, in whole or in part, is prohibited except under
# license from Versity, Inc. For more information about licensing terms,
# email info@versity.com.
#
#    VSM_notice_end

set -e

RPM_DIR=${RPM_DIR:-"$(pwd)/rpmbuild"}

SOURCE_TAR=$1
SOURCE_SPEC=$2

if [ -z "$SOURCE_TAR" ] || [ -z "$SOURCE_SPEC" ]; then
    echo "usage: $0 /path/to/pkg_source.tar.gz /path/to/pkg.spec"
    exit 1
fi

if ! test -f "$SOURCE_TAR" || ! test -f "$SOURCE_SPEC"; then
    echo "could not find file '$SOURCE_TAR' or '$SOURCE_SPEC'"
    exit 1
fi

if ! test -d "$RPM_DIR"; then
    echo "RPM_DIR '$RPM_DIR' is not valid"
    exit 1
fi

mkdir -p "${RPM_DIR}/"{SOURCES,BUILD,BUILDROOT,RPMS,SRPMS}

# find potential version files for us to extract & use
VERF=$(tar tf "$SOURCE_TAR" | egrep 'GIT-VERSION-FILE|VERSITY-VERSION')

N_VERF=$(echo "$VERF" | wc -l)

if test "$N_VERF" -ne 1; then
    echo "found '$N_VERF' version files found, confused and giving up"
    echo "files: '$VERF'"
    exit 1
fi

# now extract the version file so we can parse and use
tar -x -C "$RPM_DIR/SOURCES" -f "$SOURCE_TAR"

VERSION_FILE="$RPM_DIR/SOURCES/$VERF"

if ! test -f "$VERSION_FILE"; then
    echo "missing version file $VERSION_FILE, bailing"
    exit 1
fi

# mock builds look in SOURCES, so make sure that is valid for SOURCE_TAR
SOURCE_FILE=$(basename "$SOURCE_TAR")
if ! test "$SOURCE_TAR" -ef "$RPM_DIR/SOURCES/$SOURCE_FILE"; then
    cp -vf "$SOURCE_TAR" "$RPM_DIR/SOURCES/$SOURCE_FILE"
fi

FULL_VERSION=$(awk -F ' = ' '/FULL_VERSION/ {print $NF}' "$VERSION_FILE")
VERSION=$(awk -F ' = ' '/^VERSION/ {print $NF}' "$VERSION_FILE")

# we require a convention that is pkg_name.spec, so use it...
SPEC_NAME=$(basename "$SOURCE_SPEC")
PKG_NAME="${SPEC_NAME%.*}"

# To find build requirements
VSM_REPO=${VSM_REPO:-"vsm_master"}

# local build dependencies (libvsm, etc)
LOCAL_RPMS=${LOCAL_RPMS:-""}

# env flags to change RPM build behavior
DEV_BUILD=${DEV_BUILD:-"no"}
# is this an official release RPM ?
REL_BUILD=${REL_BUILD:-"no"}
# matches vsm 'DEBUG', turns on debug builds of source
DEBUG=${DEBUG:-"no"}
DEBUG_KERN=${DEBUG_KERN:-"no"}

# turn off safety for speed
UNCLEAN_MOCK=${UNCLEAN_MOCK:-"no"}

RESULT_DIR=${RESULT_DIR:-"$(pwd)/$PKG_NAME-$FULL_VERSION"}
mkdir -p "${RESULT_DIR}"

# mock ignores $HOME/.rpmmacros, so set here
VENDOR="Versity Software, Inc."

test_flag() {
    test "$1" == "yes"
}

string_flag() {
    if test_flag "$1"; then
        echo "true"
    else
        echo "false"
    fi
}

# print flag value and status to command line/logs
print_vars_and_flags() {
    echo
    echo "Variables:"

    echo "SOURCE_TAR: -> '$SOURCE_TAR'"
    echo "SOURCE_SPEC: -> '$SOURCE_SPEC'"
    echo "PKG_NAME: -> '$PKG_NAME'"
    echo "FULL_VERSION: -> '$FULL_VERSION'"
    echo "DISTRO_VERS: -> '$DISTRO_VERS'"
    echo "MOCK_CONFIG: -> '$MOCK_CONFIG'"
    echo "RESULT_DIR: -> '$RESULT_DIR'"
    echo "RPM_DIR: -> '$RPM_DIR'"
    echo "KVERSION: -> '$KVERSION'"

    echo
    echo "Flags:"
    echo "DEV_BUILD: -> $DEV_BUILD ($(string_flag "$DEV_BUILD"))"
    echo "REL_BUILD: -> $REL_BUILD ($(string_flag "$REL_BUILD"))"
    echo "DEBUG: -> $DEBUG ($(string_flag "$DEBUG"))"
    echo "DEBUG_KERN: -> $DEBUG_KERN ($(string_flag "$DEBUG_KERN"))"
    echo "UNCLEAN_MOCK: -> $UNCLEAN_MOCK ($(string_flag "$UNCLEAN_MOCK"))"
    echo

}

mock_init() {
    if ! test_flag "$UNCLEAN_MOCK"; then
        eval mock "$MOCK_OPTS" --init
    fi
}

mock_build () {
    OPTARGS=""

    if [ "${KVERSION}" != "" ]; then
        OPTARGS="$OPTARGS --define 'kversion ${KVERSION}'"
    fi

    if test_flag "$DEBUG"; then
        OPTARGS="$OPTARGS --define 'debugbuild 1'"
    fi

    if test_flag "$DEV_BUILD"; then
        OPTARGS="$OPTARGS --define 'devpatch $PKG_NAME-$FULL_VERSION-devpatch.diff'"
        OPTARGS="$OPTARGS --define 'debugbuild 1'"
        # TODO if developer build, add this --disable-plugin=package_state
    fi

    if test_flag "$REL_BUILD"; then
        OPTARGS="$OPTARGS --define '_release 1'"
    fi

    if test_flag "$DEBUG_KERN"; then
        OPTARGS="$OPTARGS --define 'kerndebug 1'"
    fi

    if test_flag "$UNCLEAN_MOCK"; then
        OPTARGS="$OPTARGS --no-clean --no-cleanup-after"
    fi

    # other macros to add
    OPTARGS="$OPTARGS --define 'vendor ${VENDOR}'"

    echo "MOCK_OPTS: $MOCK_OPTS"
    echo "OPTARGS: $OPTARGS"

    eval mock --buildsrpm "${MOCK_OPTS}" \
    "${OPTARGS}" \
    --spec "$SOURCE_SPEC" --sources "${RPM_DIR}/SOURCES" \
    --resultdir "${RESULT_DIR}" --no-cleanup-after \
    --disable-plugin=package_state

    if [[ -n "$LOCAL_RPMS" ]]; then
        eval mock "${MOCK_OPTS}" "${OPTARGS}" --install "${LOCAL_RPMS}"/*.x86_64.rpm
    fi

    eval mock --rebuild "${MOCK_OPTS}" \
    "${OPTARGS}" \
    --resultdir "${RESULT_DIR}" --no-clean \
    "${RESULT_DIR}/$PKG_NAME-$VERSION"*.src.rpm
}

common_build () {
    MOCK_OPTS=" --uniqueext=${USER} -r ${MOCK_CONFIG}"

    # sanity check - can't dev & rel at the same time
    if test_flag "$REL_BUILD" && test_flag "$DEV_BUILD"; then
        echo "Can't run both developer and release builds at the same time"
        exit 1
    fi

    if [[ "$VSM_REPO" != "no" ]]; then
        MOCK_OPTS+=" --enablerepo ${VSM_REPO}"
    fi

    print_vars_and_flags

    mock_init && mock_build
}

# Build against Centos 6.4
build_centos_64() {
    MOCK_CONFIG=${MOCK_CONFIG:-"centos-6.4-x86_64"}
    KVERSION=${KVERSION:-"2.6.32-358.23.2.el6"}
    common_build
}

# Centos 6.5
build_centos_65() {
    MOCK_CONFIG=${MOCK_CONFIG:-"centos-6.5-x86_64"}
    KVERSION=${KVERSION:-"2.6.32-431.29.2.el6"}
    common_build
}

# Centos 6.6
build_centos_66() {
    MOCK_CONFIG=${MOCK_CONFIG:-"centos-6.6-x86_64"}
    KVERSION=${KVERSION:-"2.6.32-504.23.4.el6"}
    common_build
}

# Centos 6.7
build_centos_67() {
    MOCK_CONFIG=${MOCK_CONFIG:-"centos-6.7-x86_64"}
    KVERSION=${KVERSION:-"2.6.32-573.12.1.el6"}
    common_build
}

# Centos 6.x (latest)
build_centos_6x() {
    MOCK_CONFIG=${MOCK_CONFIG:-"centos-6.x-x86_64"}
    KVERSION=${KVERSION:-"2.6.32-573.el6"}
    common_build
}

# Users can set the DISTRO_VERS environment variable to choose
# which CentOS distro to build against.  There is no command
# line option for this functionality.
DISTRO_VERS=${DISTRO_VERS:-6.6}

case "$DISTRO_VERS" in
 "6.4")
    build_centos_64
    ;;
 "6.5")
    build_centos_65
    ;;
 "6.6")
    build_centos_66
    ;;
 "6.7")
    build_centos_67
    ;;
 "6.x")
    build_centos_6x
    ;;
 *)
    echo "DISTRO_VERS must be one of: 6.4 6.5 6.6 6.7 6.x"
    echo "You specified DISTRO_VERS=\"${DISTRO_VERS}\""
    exit 1
    ;;
esac

if test -f test/check.sh; then
    # make sure the RPMS conform
    bash test/check.sh "${RESULT_DIR}"
    rc=$?

    if [ "$rc" -ne 0 ]; then
      echo "RPMS build ok, but install checks fail rc=$rc"
    fi
    exit $rc
else
    exit 0
fi
