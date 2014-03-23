cmake_minimum_required (VERSION 2.6)
project (maytrics)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})
set(PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(PROJECT_CODE_DIR ${PROJECT_SOURCE_DIR}/src)
set(PROJECT_VENDOR_DIR ${PROJECT_SOURCE_DIR}/vendor)

set(EVHTP_DISABLE_SSL on)
add_subdirectory(${PROJECT_VENDOR_DIR}/libevhtp)



set (maytrics_VERSION_MAJOR 0)
set (maytrics_VERSION_MINOR 1)

configure_file (
  "${PROJECT_SOURCE_DIR}/maytrics_config.h.in"
  "${PROJECT_INCLUDE_DIR}/maytrics_config.h"
  )

include_directories("${PROJECT_BINARY_DIR}")

file(
        GLOB_RECURSE
        source_files
        ${PROJECT_CODE_DIR}/*
)

# add the executable
add_executable(maytrics ${source_files})

target_link_libraries (maytrics evhtp event pthread leveldb jansson)
