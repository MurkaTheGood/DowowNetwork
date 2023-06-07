cmake_minimum_required(VERSION 3.10)

# use CTest
include(CTest)

# use debugging
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g --std=c++11")

# test executables
add_executable(Server Server.cpp)
add_executable(ConnectivityTest ConnectivityTest.cpp)
add_executable(PushTest PushTest.cpp)

target_link_libraries(Server DowowNetwork)
target_link_libraries(ConnectivityTest DowowNetwork)
target_link_libraries(PushTest DowowNetwork)

# add the test themselves
# add_test(NAME ConnectivityTest COMMAND ClientServerMetatest)
