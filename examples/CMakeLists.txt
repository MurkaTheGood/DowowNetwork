project(DowowNetworkExamples)

add_executable(ChatServer ChatServer.cpp)
add_executable(ChatClient ChatClient.cpp)

target_link_libraries(ChatServer DowowNetwork)
target_link_libraries(ChatClient DowowNetwork ncurses)
