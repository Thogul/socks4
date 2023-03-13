cmake_minimum_required(VERSION 3.10)
project(socks4)

set(CMAKE_CXX_STANDARD 17)

add_library(asio INTERFACE)
# target_compile_options(asio INTERFACE ASIO_STANDALONE)
target_include_directories(asio INTERFACE "${CMAKE_SOURCE_DIR}/lib/asio/asio/include")
target_link_libraries(asio INTERFACE pthread)
add_executable(socks4 src/main.cpp)
target_link_libraries(socks4 asio)