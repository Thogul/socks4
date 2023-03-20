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
    int maxIdLength{ 10 };

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

Socks4::Socks4(asio::io_context& io, tcp::socket newsock)
    : m_clientSock(std::move(newsock))
    , m_io(io)
    , m_serverSock(io)
    , clientBuffer(4 * 1048)
    , serverBuffer(4 * 1048)
{}

void Socks4::recvHello()
{
    // Here we read out what we need for the hello package, then connect and if that works we send
    // hello back and start listening to everything.

    // recieve the hello packet(no user id, will read that later)
    auto lambda = [self = shared_from_this()](asio::error_code ec, std::size_t bytes_read) {
        if (ec)
        {
            std::cout << "Failed handshake..." << std::endl;
            return;
        }

        if (self->m_handShakeMsg.version != 0x04)
        {
            std::cout << "Wrong SOCKS version, got " << self->m_handShakeMsg.version << std::endl;
            return;
        }

        if (self->m_handShakeMsg.cmd != 0x01)
        {
            std::cout << "Not the correct command, got " << self->m_handShakeMsg.cmd << std::endl;
            return;
        }

        // casting ip
        std::uint32_t number = self->m_handShakeMsg.destination;
        int last = (number >> (8 * 0)) & 0xff;
        int third = (number >> (8 * 1)) & 0xff;
        int scnd = (number >> (8 * 2)) & 0xff;
        int first = (number >> (8 * 3)) & 0xff;
        std::uint32_t converted = first | (scnd << 8) | (third << 16) | (last << 24);
        self->m_handShakeMsg.destination = converted;
        // auto pre = reinterpret_cast<std::uint8_t[]>(self->m_handShakeMsg.destination);

        // converting port:
        std::uint16_t nport = self->m_handShakeMsg.port;
        int plast = (nport >> (8 * 0)) & 0xff;
        int pthird = (nport >> (8 * 1)) & 0xff;
        std::uint16_t convertedPort = pthird | (plast << 8);
        self->m_handShakeMsg.port = convertedPort;


        std::cout << "Got a message: " << std::endl
                  << "msg version: " << static_cast<int>(self->m_handShakeMsg.version) << std::endl
                  << "msg cmd: " << static_cast<int>(self->m_handShakeMsg.cmd) << std::endl
                  << "msg port: " << static_cast<int>(self->m_handShakeMsg.port) << std::endl
                  << "msg dst: "
                  //   << asio::ip::address_v4(self->m_handShakeMsg.destination).to_string()
                  << asio::ip::address_v4(self->m_handShakeMsg.destination).to_string()
                  << std::endl;

        // std::cout << "Got a message: " << std::endl
        //           << "msg version: " << self->m_handShakeMsg.version << std::endl
        //           << "msg cmd: " << self->m_handShakeMsg.cmd << std::endl
        //           << "msg port: " << self->m_handShakeMsg.port << std::endl
        //           << "msg dst: " << self->m_handShakeMsg.destination << std::endl;

        // got handshake, now stored in m_handShakeMsg, now we get the user id:
        self->recvUserId();
    };

    m_clientSock.async_read_some(asio::buffer(&m_handShakeMsg, sizeof(m_handShakeMsg)), lambda);
}

void Socks4::recvUserId()
{
    std::cout << "recvning user id" << std::endl;

    auto lambda = [self = shared_from_this()](asio::error_code ec, std::size_t bytes_read) {
        if (ec)
        {
            std::cout << "Error while reading userId " << ec.message() << std::endl;
            std::cout << "read " << bytes_read << " bytes" << std::endl;
            return;
        }
        self->m_userIdSize += bytes_read;
        if (self->m_userIdSize > self->maxIdLength)
        {
            std::cout << "Too long userId!" << std::endl;
            return;
        }

        std::cout << "got " << bytes_read << " bytes" << std::endl;

        int terminatorIndex = self->findUserIdTerminator(self->m_userIdSize);
        if (terminatorIndex >= 0)
        {
            // Got a user id, get it out then connect!
            std::for_each(self->clientBuffer.begin(),
                          self->clientBuffer.begin() + terminatorIndex,
                          [self = self->shared_from_this()](std::uint8_t data) {
                              self->userId.push_back(data);
                          });
            std::cout << "have user name: " << self->userId << std::endl;
            self->connectToServer();
        }
        else
        {
            self->recvUserId();  // go again
        }
    };

    m_clientSock.async_read_some(asio::buffer(clientBuffer), lambda);
    return;
}

int Socks4::findUserIdTerminator(int bufferSize)
{
    // Checks client buffer for nullterminator, returns the index if it finds one, -1 if nothing is
    // found
    for (auto it = clientBuffer.begin(); it != clientBuffer.begin() + bufferSize; ++it)
    {
        if (*it == '\0')
        {
            return it - clientBuffer.begin();
        }
    }
    return -1;
}

void Socks4::connectToServer()
{
    asio::error_code ec{};
    auto endpoint = tcp::endpoint(asio::ip::address_v4(m_handShakeMsg.destination),
                                  m_handShakeMsg.port);  // localhost port 1338

    std::cout << "connecting to: " << asio::ip::address_v4(m_handShakeMsg.destination).to_string()
              << ":" << m_handShakeMsg.port << std::endl;
    m_serverSock.connect(endpoint, ec);  // Maybe do async connect perhaps?
    if (ec)
    {
        std::cout << "Could not connect!" << std::endl;
        m_handShakeMsg.cmd = 0x5B;
    }
    else
    {
        std::cout << "connected to server!" << std::endl;
        m_handShakeMsg.version = 0x00;
        m_handShakeMsg.cmd = 0x5A;
    }
    sendhello();  // send hello, if there are errors, that will be taken care of in send hello
}

void Socks4::sendhello()
{
    // Here we send an hellopacket and then start to recv or stop(if there was an error)
    asio::error_code ec{};
    auto error = false;
    m_clientSock.write_some(asio::buffer(&m_handShakeMsg, sizeof(m_handShakeMsg)), ec);
    if (m_handShakeMsg.cmd == 0x5A)
    {
        recvFromClient();
        recvFromServer();
    }
    else
    {
        std::cout << "There was an error in sendhello" << m_handShakeMsg.cmd << std::endl;
        return;
    }
}

void Socks4::recvFromClient()
{
    // Want som form of timeout setup
    // setClientTimeout(); or something
    auto lambda = [self = shared_from_this()](asio::error_code ec, std::size_t bytes_transferred) {
        // std::cout << "got " << bytes_transferred << " bytes of data from client" << std::endl;
        // std::for_each(self->clientBuffer.begin(),
        //               self->clientBuffer.begin() + bytes_transferred,
        //               [](const uint8_t n) { std::cout << n; });
        // std::cout << std::endl;
        if (ec)
        {
            // do some error stuff...
            std::cout << "got error in recvFromClient: " << ec.message() << std::endl;
            return;
        }
        else
        {
            self->sendToServer(bytes_transferred);
        }
    };
    // std::cout << "recieving from client.." << std::endl;
    m_clientSock.async_read_some(asio::buffer(clientBuffer), lambda);
}

void Socks4::recvFromServer()
{
    // Want som form of timeout setup
    // setClientTimeout(); or something
    auto lambda = [self = shared_from_this()](asio::error_code ec, std::size_t bytes_transferred) {
        // std::cout << "got " << bytes_transferred << " bytes of data from server" << std::endl;
        // std::for_each(self->serverBuffer.begin(),
        //               self->serverBuffer.begin() + bytes_transferred,
        //               [](const uint8_t n) { std::cout << n; });
        // std::cout << std::endl;
        if (ec)
        {
            // do some error stuff...
            std::cout << "Got error recvFromServer: " << ec.message() << std::endl;
            return;
        }
        else
        {
            self->sendToClient(bytes_transferred);
        }
    };
    // std::cout << "recieving from client.." << std::endl;
    m_serverSock.async_read_some(asio::buffer(serverBuffer), lambda);
}

void Socks4::sendToClient(std::size_t bytes)
{
    auto lambda = [self = shared_from_this()](asio::error_code ec, std::size_t bytes_transferred) {
        // std::cout << "wrote " << bytes_transferred << "to client" << std::endl;
        if (ec)
        {
            // do some error
            std::cout << "Got error in sendToClient: " << ec.message() << std::endl;
            return;
        }
        else
        {
            self->recvFromServer();
        }
    };
    // std::cout << "writing to server..." << std::endl;
    m_clientSock.async_write_some(asio::buffer(serverBuffer, bytes), lambda);
}

void Socks4::sendToServer(std::size_t bytes)
{
    auto lambda = [self = shared_from_this()](asio::error_code ec, std::size_t bytes_transferred) {
        // std::cout << "wrote " << bytes_transferred << "to server" << std::endl;
        if (ec)
        {
            // do some error
            std::cout << "Got error in sendToServer: " << ec.message() << std::endl;
            return;
        }
        else
        {
            self->recvFromClient();
        }
    };
    // std::cout << "writing to server..." << std::endl;
    m_serverSock.async_write_some(asio::buffer(clientBuffer, bytes), lambda);
}
}  // namespace socks
