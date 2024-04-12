#
# Copyright 2022 Max Planck Institute for Software Systems, and
# National University of Singapore
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

cmake_minimum_required(VERSION 3.16.3)

project(simbricks-trace LANGUAGES CXX DESCRIPTION "simbricks trace utility for analyzing system performance in depth")

#######################################
# Build Type, by default build Release
# can be spedcified by -DCMAKE_BUILD_TYPE=Debug or -DCMAKE_BUILD_TYPE=Release
#######################################
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Build_Type Debug")
    set(CMAKE_BUILD_TYPE Debug)
    add_compile_options(-fno-omit-frame-pointer)
else()
    message(STATUS "Build_Type Release")
    set(CMAKE_BUILD_TYPE Release)
    add_compile_options(-O3)
endif()

option(NATIVE "set -march=native compiler flag for generating processor family specific code for better performance" OFF)
if(NATIVE)
    message(STATUS "ENABLED -march=native")
    add_compile_options(-march=native)
else()
    message(STATUS "DISABLED -march=native")
endif()

#######################################
# Required C standard
#######################################
if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#######################################
# Option to enable address and UB sanitizers
#######################################
option(ENABLE_SANITIZER "Enable address and UB sanitizers" OFF)
if (ENABLE_SANITIZER)
    message(STATUS "ENABLED address and UB sanitizers")
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
else ()
    message(STATUS "DISABLED address and UB sanitizers")
endif ()

#######################################
# Use ccache by default if installed on the system
#######################################
option(ENABLE_CCACHE "enable usage of ccache if available on the system" ON)
if (ENABLE_CCACHE)
    message(STATUS "Try using ccache when building")
    find_program(CCACHE_FOUND ccache)
    if(CCACHE_FOUND)
        message(STATUS "Ccache found on the system, enable it for linking and compiling")
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
    else ()
        message(STATUS "Ccache not found on the system")
    endif()
else ()
    message(STATUS "Disabled ccache usage")
endif ()

#######################################
# Catch 2
#######################################
Include(FetchContent)
FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.3.2 # or a later release
    )
FetchContent_MakeAvailable(Catch2)

#######################################
# Spdlog
#######################################
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.12.0
)
FetchContent_MakeAvailable(spdlog)

#######################################
# Yaml-Cpp
#######################################
FetchContent_Declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG yaml-cpp-0.7.0
)
set(YAML_CPP_BUILD_TESTS OFF)
FetchContent_MakeAvailable(yaml-cpp)

#######################################
# Concurrencpp
#######################################
add_subdirectory(libs/concurrencpp)


#######################################
# Opentelemetry cpp client
#######################################
add_subdirectory(libs/opentelemetry-cpp)
include_directories(
        libs/opentelemetry-cpp/exporters/otlp/include
        libs/opentelemetry-cpp/exporters/ostream/include
)


#######################################
# The Trace lib
#######################################
# all src files required to build the trace functionality
set(TRACE_LIB_HEADER_FILES
        #utils
        include/util/exception.h
        include/util/componenttable.h
        include/util/cxxopts.hpp
        include/util/log.h
        include/util/factory.h
        include/util/utils.h
        include/util/concepts.h
        include/util/ttlMap.h
        # channel abstractions
        include/sync/channel.h
        # coroutine wrappers
        include/sync/corobelt.h
        # channel and corobelt template specializations for our use case
        include/sync/specializations.h
        # reader
        include/reader/cReader.h
        # parser
        include/parser/parser.h
        # events
        include/events/eventType.h
        include/events/eventTimeBoundary.h
        include/events/events.h
        include/events/event-filter.h
        include/parser/eventStreamParser.h
        include/events/printer.h
        # config
        include/config/config.h
        # tracing environment
        include/env/stringInternalizer.h
        include/env/symtable.h
        include/env/traceEnvironment.h
        # analytics
        include/analytics/span.h
        include/analytics/trace.h
        include/analytics/tracer.h
        include/analytics/context.h
        include/analytics/traceContext.h
        include/analytics/spanner.h
        include/analytics/timer.h
        include/analytics/helper.h
        # exporter
        include/exporter/exporter.h
        )
set(TRACE_LIB_SOURCE_FILES
        # reader
        source/reader/cReader.cpp
        # parser
        source/parser/parser.cc
        source/parser/nicbm.cc
        source/parser/gem5.cc
        source/parser/ns3.cpp
        source/parser/event-stream.cpp
        # events
        source/events/events.cc
        # tracing environment
        source/env/symtable.cc
        source/env/traceEnvironment.cc
        # analytics
        source/analytics/span.cpp
        source/analytics/spanner.cpp
        source/analytics/nicSpanner.cc
        source/analytics/hostSpanner.cc
        source/analytics/networkSpanner.cpp
        )
add_library(${PROJECT_NAME} ${TRACE_LIB_HEADER_FILES} ${TRACE_LIB_SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC
        include
        source
        )
# Check whether the correct compiler is used
message(STATUS "The used compiler id: ${CMAKE_CXX_COMPILER_ID}")
# only gcc is supported at the moment
if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    target_compile_options(${PROJECT_NAME} PUBLIC -pthread)
elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PUBLIC -pthread)
else()
    message(FATAL_ERROR "Your compiler, whatever it is, is currently not supported.")
endif()
# link coroutine library
target_link_libraries(${PROJECT_NAME} PUBLIC
        yaml-cpp
        spdlog::spdlog
        concurrencpp::concurrencpp
        Catch2::Catch2WithMain
        opentelemetry_trace
        opentelemetry_api
        opentelemetry_exporter_otlp_http
        opentelemetry_exporter_ostream_span
)


#######################################
# Trace - Tests
#######################################
set(UNIT_TESTS_TARGET unittests)
set (TRACE_UNIT_TESTS_HEADER_FILES
    tests/test-util.h
    )
set (TRACE_UNIT_TESTS_SRC_FILES
        tests/channel-test.cpp
        tests/pipeline-test.cpp
        tests/span-test.cpp
        tests/gem5-parser-test.cpp
        tests/ns3-parser-test.cpp
        tests/event-stream-parser-test.cpp
        tests/reader-test.cpp
        tests/config-test.cpp
        )
add_executable(${UNIT_TESTS_TARGET} ${TRACE_UNIT_TESTS_HEADER_FILES} ${TRACE_UNIT_TESTS_SRC_FILES})
target_include_directories(${UNIT_TESTS_TARGET} PUBLIC
        tests
        )
target_link_libraries(${UNIT_TESTS_TARGET} PUBLIC Catch2::Catch2WithMain)
target_link_libraries(${UNIT_TESTS_TARGET} PUBLIC ${PROJECT_NAME})

#######################################
# Trace executable
#######################################
set(TRACE_SRC_FILES
    trace.cc
   )
add_executable(trace ${TRACE_SRC_FILES})
target_link_libraries(trace ${PROJECT_NAME})

#######################################
# Trace utility binary to clear named
# pipes for usage during checkpointing
# phase when performing online tracing
# with gem5
#######################################
set(TRACE_EXHAUST_UTIL_FILES
    namedpipe-exhauster.cpp
)
add_executable(exhauster ${TRACE_EXHAUST_UTIL_FILES})
target_link_libraries(exhauster ${PROJECT_NAME})
