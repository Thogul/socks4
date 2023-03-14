#pragma once

#include <memory.h>
#include <iostream>

#include <asio.hpp>


namespace socks {
using asio::ip::tcp;

class Socks4 : public std::enable_shared_from_this<Socks4>
{
    // This is the socks4 session.
    // Gets a open client socket from the manager, reads hello, connects to server, sends hello back
    // to client then passes data along from client <-> server

  private:
    tcp::socket m_clientSock;
    // tcp::socket m_serverSock;

  public:
    Socks4(tcp::socket newSock);

    void recvHello();
    void sendhello();
    void recvFromClient();
    void recvFromServer();
    void sendToClient();
    void sendToServer();
};

Socks4::Socks4(tcp::socket newsock) : m_clientSock(std::move(newsock))
{
    std::string msg = "Hello world!";
    m_clientSock.async_write_some(asio::buffer(msg),
                                  [](const asio::error_code& error,  // Result of operation.
                                     std::size_t bytes_transferred) {
                                      std::cout << "Wrote: " << bytes_transferred << " bytes"
                                                << std::endl;
                                  });
}
}  // namespace socks
