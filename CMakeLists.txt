cmake_minimum_required(VERSION 3.2)

include("cmake/HunterGate.cmake")
HunterGate(
    URL "https://github.com/cpp-pm/hunter/archive/v0.23.262.tar.gz"
    SHA1 "eb51e633e08cdbe2153caf255e9c23968fecb29d"
)

project(s3 VERSION 4.1)

add_library(
    s3
    src/bucket.cpp
    src/bucket_metadata.c
    src/error_parser.c
    src/general.c
    src/multipart.cpp
    src/object.c
    src/request.cpp
    src/request_context.cpp
    src/response_headers_handler.cpp
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
    _CRT_SECURE_NO_WARNINGS
)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
  target_compile_definitions(s3 PRIVATE _POSIX_C_SOURCE=200112L)
endif()

if(NOT MSVC)  ## Useful as a linker symbols validation test that someone didn't forget to annotate extern "C"
  add_executable(s3exe
      src/s3.c
  )
  target_link_libraries(s3exe PRIVATE s3)
  set_target_properties(s3exe PROPERTIES OUTPUT_NAME "s3")
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
