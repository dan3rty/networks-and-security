add_executable(lab6
        src/WebServer.h
        src/WebServer.cpp
        src/main.cpp)

add_custom_command(
        TARGET lab6 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/public
        ${CMAKE_CURRENT_BINARY_DIR}/public
        COMMENT "Copying 'public' directory next to executable"
)