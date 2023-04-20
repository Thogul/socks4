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
}  // namespace socks