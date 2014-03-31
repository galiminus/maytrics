include(ExternalProject)

cmake_minimum_required (VERSION 2.6)
project (maytrics)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})
set(PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(PROJECT_CODE_DIR ${PROJECT_SOURCE_DIR}/src)
set(CMAKE_C_COMPILER g++)
set(CMAKE_C_FLAGS -fpermissive)

set (maytrics_VERSION_MAJOR 0)
set (maytrics_VERSION_MINOR 1)

configure_file (
  "${PROJECT_SOURCE_DIR}/maytrics_config.h.in"
  "${PROJECT_INCLUDE_DIR}/maytrics_config.h"
  )

include_directories("${PROJECT_BINARY_DIR}"
                    "${PROJECT_SOURCE_DIR}/libevhtp-prefix/src/libevhtp/"
                    "${PROJECT_SOURCE_DIR}/libevhtp-prefix/src/libevhtp/htparse"
                    "${PROJECT_SOURCE_DIR}/libevhtp-prefix/src/libevhtp/oniguruma")

file(
        GLOB_RECURSE
        source_files
        ${PROJECT_CODE_DIR}/*
)

set(EVHTP_DISABLE_SSL on)
ExternalProject_add(libevhtp
  GIT_REPOSITORY https://github.com/phorque/libevhtp.git
  GIT_TAG 1.2.9
  INSTALL_COMMAND ""
  BUILD_IN_SOURCE ON
  CMAKE_ARGS -DEVHTP_DISABLE_SSL:STRING=ON -DEVHTP_DISABLE_EVTHR:STRING=ON
)

# add the executable
add_executable(maytrics ${source_files})

add_dependencies(maytrics libevhtp)

target_link_libraries (maytrics event leveldb snappy jansson
                      ${PROJECT_SOURCE_DIR}/libevhtp-prefix/src/libevhtp/libevhtp.a
                      pthread
)
