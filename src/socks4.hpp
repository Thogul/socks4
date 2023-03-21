#pragma once

#include <memory.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string.h>

#include <asio.hpp>


namespace socks {
using asio::ip::tcp;

struct HandshakeMsg
{
    uint8_t version;
    uint8_t cmd;
    uint16_t port;
    uint32_t destination;
};

class Socks4 : public std::enable_shared_from_this<Socks4>
{
    // This is the socks4 session.
    // Gets a open client socket from the manager, reads hello, connects to server, sends hello back
    // to client then passes data along from client <-> server

  private:
    asio::io_context& m_io;
    tcp::socket m_clientSock;
    tcp::socket m_serverSock;
    std::vector<uint8_t> clientBuffer;
    std::vector<uint8_t> serverBuffer;
    HandshakeMsg m_handShakeMsg;
    std::string userId;
    std::uint8_t m_userIdSize;
    int maxIdLength{ 1000 };

  public:
    Socks4(asio::io_context& io, tcp::socket newSock);

    void recvHello();
    void recvUserId();
    void sendhello();
    void recvFromClient();
    void recvFromServer();
    void sendToClient(std::size_t bytes);
    void sendToServer(std::size_t bytes);
    int findUserIdTerminator(int bufferSize);
    void connectToServer();
};

}  // namespace socks
