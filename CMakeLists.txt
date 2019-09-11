cmake_minimum_required(VERSION 3.2)

include("cmake/HunterGate.cmake")
HunterGate(
    URL "https://github.com/ruslo/hunter/archive/v0.23.214.tar.gz"
    SHA1 "e14bc153a7f16d6a5eeec845fb0283c8fad8c358"
)

project(s3 VERSION 4.1)

add_library(
    s3
    src/bucket.c
    src/bucket_metadata.c
    src/error_parser.c
    src/general.c
    src/multipart.c
    src/object.c
    src/request.c
    src/request_context.c
    src/response_headers_handler.c
    src/service.c
    src/service_access_logging.c
    src/simplexml.c
    src/util.c
)

hunter_add_package(libxml2)
find_package(libxml2 CONFIG REQUIRED)
target_link_libraries(s3 PUBLIC libxml2::libxml2)

hunter_add_package(CURL)
find_package(CURL CONFIG REQUIRED)
target_link_libraries(s3 PUBLIC CURL::libcurl)

target_include_directories(
    s3 PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/inc>
)

target_compile_definitions(
    s3
    PRIVATE
    LIBS3_VER_MAJOR="${PROJECT_VERSION_MAJOR}"
    LIBS3_VER_MINOR="${PROJECT_VERSION_MINOR}"
    LIBS3_VER="${PROJECT_VERSION}"
    __STRICT_ANSI__
    _ISOC99_SOURCE
)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
  target_compile_definitions(s3 PRIVATE _POSIX_C_SOURCE=200112L)
endif()

### Installation (https://github.com/forexample/package-example) {

# Introduce variables:
# * CMAKE_INSTALL_LIBDIR
# * CMAKE_INSTALL_BINDIR
# * CMAKE_INSTALL_INCLUDEDIR
include(GNUInstallDirs)

set(config_install_dir "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(version_config "${generated_dir}/${PROJECT_NAME}ConfigVersion.cmake")
set(project_config "${generated_dir}/${PROJECT_NAME}Config.cmake")
set(TARGETS_EXPORT_NAME "${PROJECT_NAME}Targets")
set(NAMESPACE "${PROJECT_NAME}::")

include(CMakePackageConfigHelpers)

# Use:
# * PROJECT_VERSION
write_basic_package_version_file(
    "${version_config}" COMPATIBILITY SameMajorVersion
)

# Use:
# * TARGETS_EXPORT_NAME
# * PROJECT_NAME
configure_package_config_file(
    "cmake/template/Config.cmake.in"
    "${project_config}"
    INSTALL_DESTINATION "${config_install_dir}"
)

install(
    FILES "inc/libs3.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
    FILES "${project_config}" "${version_config}"
    DESTINATION "${config_install_dir}"
)

install(
    EXPORT "${TARGETS_EXPORT_NAME}"
    NAMESPACE "${NAMESPACE}"
    DESTINATION "${config_install_dir}"
)

install(
    TARGETS s3
    EXPORT "${TARGETS_EXPORT_NAME}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

### }
