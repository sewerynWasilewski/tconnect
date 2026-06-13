add_executable(test_transport test_transport.c)
target_link_libraries(test_transport tconnect)
add_test(NAME transport COMMAND test_transport)
