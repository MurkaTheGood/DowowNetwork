cmake_minimum_required(VERSION 3.10)

# use CTest
include(CTest)

# use debugging
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g --std=c++11")

# test executables
add_executable(Server Server.cpp)
add_executable(ConnectivityTest ConnectivityTest.cpp)
add_executable(PullTest PullTest.cpp)
add_executable(PushTest PushTest.cpp)
add_executable(ClientTest ClientTest.cpp)

target_link_libraries(Server DowowNetwork)
target_link_libraries(ConnectivityTest DowowNetwork)
target_link_libraries(PullTest DowowNetwork)
target_link_libraries(PushTest DowowNetwork)
target_link_libraries(ClientTest DowowNetwork)

# add the test themselves
find_program(BASH_PROGRAM bash)
if (BASH_PROGRAM)
    add_test(NAME ConnectivityTest COMMAND
        ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/ServerPairTest.sh ConnectivityTest)
    add_test(NAME PullTest COMMAND
        ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/ServerPairTest.sh PullTest)
    add_test(NAME PushTest COMMAND
        ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/ServerPairTest.sh PushTest)
    add_test(NAME ClientTest COMMAND
        ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/ServerPairTest.sh ClientTest)
endif()
