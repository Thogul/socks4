#pragma once

#include <memory.h>
#include <iostream>

#include <asio.hpp>

#include "socks4.hpp"

namespace socks {

using asio::ip::tcp;

class Manager : public std::enable_shared_from_this<Manager>
{
    // Listens for new connections and manages/creates socks4 session on new connections
  private:
    asio::io_context& m_io;
    tcp::acceptor m_acceptor;

  public:
    Manager(asio::io_context& io);
    std::shared_ptr<Manager> getptr();
    void startAccept();
    void handleAccept();
    void createSession(asio::io_context& io, tcp::socket soc);
};

std::shared_ptr<Manager> Manager::getptr() { return shared_from_this(); }

Manager::Manager(asio::io_context& io) : m_io(io), m_acceptor(io, tcp::endpoint(tcp::v4(), 1080)){};

void Manager::startAccept()
{
    std::cout << "Start accept" << std::endl;
    tcp::socket newSock = tcp::socket{ m_io };
    // newSock.set_option(tcp::socket::reuse_address(true));

    auto lambda = [self = shared_from_this()](asio::error_code ec, tcp::socket socket) {
        if (ec)
        {
            std::cout << "Failed to accept new connection!" << ec.message() << std::endl;
            return;
        }
        else
        {
            std::cout << "New connection!" << std::endl;
        }

        // Socks4 newSession{ std::move(
        //     socket) };  // FIX, skal være shared ptr... hvorfor så mye move...
        auto newSession = std::make_shared<socks::Socks4>(self->m_io, std::move(socket));
        newSession->recvHello();  // start first coroutine

        self->startAccept();
        // [this, other = shared_from_this()](void) {
        //     startAccept();
        // }();  // Start accepting again, Prøve uten denne...
    };
    m_acceptor.async_accept(lambda);
    std::cout << "Accept setup!" << std::endl;
}
}  // namespace socks