cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

project(networks-and-security LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(tcp)
add_subdirectory(web_server)
add_subdirectory(smtp_client)
add_subdirectory(dns_resolver)
