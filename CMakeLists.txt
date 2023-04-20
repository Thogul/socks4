cmake_minimum_required(VERSION 3.10)
project(socks4)

set(CMAKE_CXX_STANDARD 20)

add_library(asio INTERFACE)
# target_compile_options(asio INTERFACE ASIO_STANDALONE)
target_include_directories(asio INTERFACE "${CMAKE_SOURCE_DIR}/lib/asio/asio/include")
target_link_libraries(asio INTERFACE pthread)
add_executable(socks4 src/main.cpp src/manager.cpp src/socks4.cpp)
target_link_libraries(socks4 asio)

add_executable(client src/client.cpp src/manager.cpp src/socks4.cpp)
target_link_libraries(client asio)