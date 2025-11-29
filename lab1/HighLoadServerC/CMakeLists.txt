cmake_minimum_required(VERSION 3.22)
project(HighLoadServerC C)

set(CMAKE_C_STANDARD 17)
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin")

add_executable(${PROJECT_NAME}
        src/main.c
        src/proto.c
        src/socket.c
        src/tcp_server.c
        src/tcp_client.c
        src/server.c
        src/client.c
        src/proto.h
        src/socket.h
        src/tcp_server.h
        src/tcp_client.h
)


target_link_libraries(${PROJECT_NAME} ws2_32)

