cmake_minimum_required(VERSION 3.10)
project(Block3 C)

set(CMAKE_C_STANDARD 99)

add_executable(client client.c)
add_executable(server server.c hash_functions.c)