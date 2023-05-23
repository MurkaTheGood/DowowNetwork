cmake_minimum_required(VERSION 3.10)

# use CTest
include(CTest)

# use debugging
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g --std=c++11")

# test executables
add_executable(ServerTest ServerTest.cpp)
add_executable(ClientTest ClientTest.cpp)
add_executable(ClientServerMetatest ClientServerMetatest.cpp)

target_link_libraries(ServerTest DowowNetwork)
target_link_libraries(ClientTest DowowNetwork)

# add the test themselves
add_test(NAME ClientServer COMMAND ClientServerMetatest)
