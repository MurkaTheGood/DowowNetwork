cmake_minimum_required(VERSION 3.10)
project(DowowNetwork)

# add some options for build configurations
option(BUILD_TESTS "Build tests?" OFF)
option(DEBUG "Include debug symbols?" OFF)
option(DEBUG_VERBOSE "Print verbose debug messages? Works if DEBUG" OFF)
option(DEBUG_DUMP_CONNECTIONS "Dump data transferred via sockets to disk? Works if DEBUG" OFF)

# sources of some value types
set(VALUES_SOURCES
    values/Value8S.cpp
    values/Value8U.cpp
    values/Value16S.cpp
    values/Value16U.cpp
    values/Value32S.cpp
    values/Value32U.cpp
    values/Value64S.cpp
    values/Value64U.cpp
    values/ValueArr.cpp
    values/ValueStr.cpp
    values/ValueUndefined.cpp
)
# sources of the main library
set(SOURCES
    Connection.cpp
    Client.cpp
    Datum.cpp
    Request.cpp
    Server.cpp
    Value.cpp
    Utils.cpp
)

# use C++11
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -Wall")

if(!DEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
    if(DEBUG_VERBOSE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVERBOSE_DEBUG")
    endif()
    if(DEBUG_DUMP_CONNECTIONS)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDUMP_CONNECTIONS")
    endif()
endif()

# generate some files using Python script
add_custom_command(
    OUTPUT values/All.hpp
    COMMAND python3 prepare.py
    DEPENDS ${VALUES_SOURCES}
    VERBATIM)
add_custom_target(All.hpp DEPENDS values/All.hpp)

# add the library itself
add_library(DowowNetwork STATIC ${SOURCES} ${VALUES_SOURCES})
add_dependencies(DowowNetwork All.hpp)

# adding the test subdirectory if tests are enabled
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
