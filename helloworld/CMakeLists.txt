cmake_minimum_required(VERSION 3.10)
project(helloworld)

set(CMAKE_C_STANDARD 99)

INCLUDE_DIRECTORIES(include)

add_executable(helloworld main.c src/foo.c include/foo.h)