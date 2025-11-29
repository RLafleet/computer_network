cmake_minimum_required(VERSION 3.22)
project(WebServerLab2 C)

set(CMAKE_C_STANDARD 17)
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin")

add_executable(${PROJECT_NAME}
        src/main.c
        src/http_server.c
        src/http_parser.c
        src/file_handler.c
        src/mime_types.c
        src/socket.c
        src/thread_pool.c
        src/http_server.h
        src/http_parser.h
        src/file_handler.h
        src/mime_types.h
        src/socket.h
        src/thread_pool.h
)

target_link_libraries(${PROJECT_NAME} ws2_32)