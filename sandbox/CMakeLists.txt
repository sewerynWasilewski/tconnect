add_executable(sandbox main.c)
target_link_libraries(sandbox tconnect)

option(SANDBOX_TLS "Build sandbox in WSS mode" OFF)
if(SANDBOX_TLS)
  target_compile_definitions(sandbox PRIVATE USE_TLS)
endif()
