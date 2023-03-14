#include <iostream>
#include <memory>
#include <asio.hpp>

#include "manager.hpp"

int main()
{
    try
    {
        asio::io_context io;
        auto manager = std::make_shared<socks::Manager>(io);
        manager->startAccept();
        io.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }

    // asio::steady_timer t(io, asio::chrono::seconds(5));
    // t.wait();
    // std::cout << "Hello, world!" << std::endl;
    return 0;
}