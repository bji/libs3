#!/bin/bash
#
#    VSM_notice_begin
#
#       VSM - Versity Storage Management File System
#
#       Copyright (c) 2015  Versity Software, Inc.
#       All Rights Reserved
#
#    VSM_notice_end

set -ex

# upstream +1 from where we started, as we have changes not upstream yet
PKG_VERSION=${PKG_VERSION:-"2.1"}

# VSM_PATCH is optional, mostly for developers
# it replaces the release field in the code, but in the RPM
# it is added after a "." to ensure RPM version correctness

VSM_PATCH=${VSM_PATCH:-"$(git rev-parse --short HEAD)"}
VSM_SOURCE_EXTRA="-${VSM_PATCH}"

# env flags to change RPM build behavior
DEV_BUILD=${DEV_BUILD:-"no"}

RESULT_DIR=${RESULT_DIR:-"$(pwd)/libs3-${PKG_VERSION}${VSM_SOURCE_EXTRA}"}
mkdir -p ${RESULT_DIR}

RPM_DIR=${RPM_DIR:-"${HOME}/rpmbuild"}
mkdir -p ${RPM_DIR}/{SOURCES,BUILD,BUILDROOT,RPMS,SRPMS}

# mock ignores $HOME/.rpmmacros, so set here
VENDOR="Versity Software, Inc."

test_flag() {
    flag=$1

    if [ "$flag" == "yes" ]; then
        /bin/true
    else
        /bin/false
    fi
}

string_flag() {
    flag=$1

    if test_flag $flag == /bin/true; then
        echo "true"
    else
        echo "false"
    fi
}

# print flag value and status to command line/logs
print_vars_and_flags() {
    echo
    echo "Variables:"

    echo "DISTRO_VERS: -> '$DISTRO_VERS'"
    echo "MOCK_CONFIG: -> '$MOCK_CONFIG'"
    echo "RESULT_DIR: -> '$RESULT_DIR'"
    echo "RPM_DIR: -> '$RPM_DIR'"
    echo "VSM_PATCH: -> '$VSM_PATCH'"
    echo

}

mock_init() {
    mock $MOCK_OPTS --init
}

mock_build () {
    # TODO would be nice to have a find + xargs + latest mtime
    #  and then only make dist if needed
    OPTARGS="--define '_version ${PKG_VERSION}'"

    if [ "${VSM_PATCH}" != "" ]; then
        OPTARGS="$OPTARGS --define 'vsm_patch ${VSM_PATCH}'"
    fi

    gmake PKG_VERSION=${PKG_VERSION} VSM_PATCH=${VSM_PATCH} dist

    # other macros to add
    OPTARGS="$OPTARGS --define 'vendor ${VENDOR}'"

    echo "MOCK_OPTS: $MOCK_OPTS"
    echo "OPTARGS: $OPTARGS"

    eval mock --buildsrpm ${MOCK_OPTS} \
    ${OPTARGS} \
    --spec $(pwd)/../libs3.spec --sources ${RPM_DIR}/SOURCES \
    --resultdir ${RESULT_DIR} --no-cleanup-after \
    --disable-plugin=package_state

    eval mock --rebuild ${MOCK_OPTS} \
    ${OPTARGS} \
    --resultdir ${RESULT_DIR} --no-clean \
    ${RESULT_DIR}/libs3-${PKG_VERSION}*${VSM_PATCH}*.src.rpm
}

common_build () {
    MOCK_OPTS=" --uniqueext=${USER} -r ${MOCK_CONFIG}"

    print_vars_and_flags

    mock_init && mock_build
}

# Build against Centos 6.4
build_centos_64() {
    MOCK_CONFIG=${MOCK_CONFIG:-"centos-6.4-x86_64"}
    common_build
}

# Centos 6.5
build_centos_65() {
    MOCK_CONFIG=${MOCK_CONFIG:-"centos-6.5-x86_64"}
    common_build
}

# Centos 6.x (latest), currently 6.6
build_centos_6x() {
    MOCK_CONFIG=${MOCK_CONFIG:-"epel-6-x86_64"}
    common_build
}

# allows users to set DISTRO_VERS environment config
DISTRO_VERS=${DISTRO_VERS:-"$1"}

case "$DISTRO_VERS" in
 "6.4")
    build_centos_64
    ;;
 "6.5")
    build_centos_65
    ;;
 "6.x")
    build_centos_6x
    ;;
 *)
    DISTRO_VERS="default"
     # Centos 6.6 is current default
    build_centos_6x
    ;;
esac
