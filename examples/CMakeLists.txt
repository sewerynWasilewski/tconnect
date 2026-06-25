foreach(example tls_raw https_get http_get_post udp_dns_query)
  add_executable(example_${example} ${example}.c)
  target_link_libraries(example_${example} tconnect)
endforeach()
