cmake_minimum_required(VERSION 3.11)

project(countd)
set(CMAKE_CXX_STANDARD 20)

set(KLS_DISABLE_TEST TRUE)
add_subdirectory(klsxx)

file(GLOB_RECURSE SRC ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.*)
add_executable(countd ${SRC})
target_link_libraries(countd PUBLIC klsxx::essential klsxx::coroutine klsxx::thread klsxx::io)
