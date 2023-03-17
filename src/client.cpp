#include <iostream>
#include <asio.hpp>
#include "socks4.hpp"

#include <vector>
#include <string>
#include <thread>
#include <chrono>

using asio::ip::tcp;


int main(int argc, char const* argv[])
{

    std::cout << "Starting client..." << std::endl;
    socks::HandshakeMsg msg;
    msg.version = 0x04;
    msg.cmd = 0x01;
    msg.port = 1338;  // 1338 port
    msg.destination = asio::ip::make_address_v4("127.0.0.1").to_uint();
    std::string userId = "Thomas";
    // std::string userId = "Thomas\0";
    asio::error_code ec;
    asio::io_context io;
    tcp::socket socket{ io };
    std::cout << "port: " << static_cast<int>(msg.port) << std::endl;
    socket.connect(tcp::endpoint(asio::ip::address_v4(msg.destination), 1337), ec);
    if (ec)
    {
        std::cout << "Got error: " << ec.message() << std::endl;
        return 1;
    }

    socket.write_some(asio::buffer(&msg, sizeof(msg)), ec);
    if (ec)
    {
        std::cout << "got error from writing: " << ec.message() << std::endl;
        return 1;
    }

    socket.write_some(asio::buffer(userId.c_str(), userId.size() + 1), ec);
    if (ec)
    {
        std::cout << "got error from sending user name: " << ec.message() << std::endl;
        return 1;
    }

    socket.read_some(asio::buffer(&msg, sizeof(msg)), ec);
    if (ec)
    {
        std::cout << "got some error from recieving handshake back..." << ec.message() << std::endl;
        return 1;
    }

    socket.write_some(asio::buffer("Hello world!"), ec);
    if (ec)
    {
        std::cout << "got some error from sending hello world!..." << ec.message() << std::endl;
        return 1;
    }

    std::vector<uint8_t> myBuffer(1000);
    for (;;)
    {
        std::size_t read = socket.read_some(asio::buffer(myBuffer), ec);
        std::for_each_n(myBuffer.begin(), read, [](uint8_t n) { std::cout << n; });
        // std::cout << std::endl;
    }
    std::this_thread::sleep_for(std::chrono::seconds(3));
    return 0;
}
