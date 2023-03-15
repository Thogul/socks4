#pragma once

#include <memory.h>
#include <iostream>
#include <vector>
#include <algorithm>

#include <asio.hpp>


namespace socks {
using asio::ip::tcp;

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

  public:
    Socks4(asio::io_context& io, tcp::socket newSock);

    void recvHello();
    void sendhello();
    void recvFromClient();
    void recvFromServer();
    void sendToClient(std::size_t bytes);
    void sendToServer(std::size_t bytes);
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
    // std::string msg = "Hello world!";
    // m_clientSock.async_write_some(asio::buffer(msg),
    //                               [](const asio::error_code& error,  // Result of operation.
    //                                  std::size_t bytes_transferred) {
    //                                   std::cout << "Wrote: " << bytes_transferred << " bytes"
    //                                             << std::endl;
    //                               });

    // Here we read out what we need for the hello package, then connect and if that works we send
    // hello back and start listening to everything.


    // For now we skip all that and try to connect to port 1338 on localhost to start everything
    asio::error_code ec{};
    auto endpoint = tcp::endpoint(tcp::v4(), 1338);  // localhost port 1338
    m_serverSock.connect(endpoint, ec);              // do async connnect actually...
    if (ec)
    {
        std::cout << "Could not connect!" << std::endl;
    }
    else
    {
        std::cout << "connected to server!" << std::endl;
        sendhello();
    }
    // sendhello();
}

void Socks4::sendhello()
{
    // Here we send an hellopacket and then start to recv or stop(if there was an error)

    auto error = false;
    if (error)
    {
        std::cout << "There was an error in sendhello" << std::endl;
        return;
    }
    else
    {
        recvFromClient();
        recvFromServer();
    }
}

void Socks4::recvFromClient()
{
    // Want som form of timeout setup
    // setClientTimeout(); or something
    auto lambda = [self = shared_from_this()](asio::error_code ec, std::size_t bytes_transferred) {
        std::cout << "got " << bytes_transferred << " bytes of data from client" << std::endl;
        std::for_each(self->clientBuffer.begin(),
                      self->clientBuffer.begin() + bytes_transferred,
                      [](const uint8_t n) { std::cout << n; });
        std::cout << std::endl;
        if (ec)
        {
            // do some error stuff...
            std::cout << "got error in recvFromClient: " << ec.message() << std::endl;
        }
        else
        {
            self->sendToServer(bytes_transferred);
        }
    };
    std::cout << "recieving from client.." << std::endl;
    m_clientSock.async_read_some(asio::buffer(clientBuffer), lambda);
}

void Socks4::recvFromServer()
{
    // Want som form of timeout setup
    // setClientTimeout(); or something
    auto lambda = [self = shared_from_this()](asio::error_code ec, std::size_t bytes_transferred) {
        std::cout << "got " << bytes_transferred << " bytes of data from server" << std::endl;
        std::for_each(self->serverBuffer.begin(),
                      self->serverBuffer.begin() + bytes_transferred,
                      [](const uint8_t n) { std::cout << n; });
        std::cout << std::endl;
        if (ec)
        {
            // do some error stuff...
            std::cout << "Got error recvFromServer: " << ec.message() << std::endl;
        }
        else
        {
            self->sendToClient(bytes_transferred);
        }
    };
    std::cout << "recieving from client.." << std::endl;
    m_serverSock.async_read_some(asio::buffer(serverBuffer), lambda);
}

void Socks4::sendToClient(std::size_t bytes)
{
    auto lambda = [self = shared_from_this()](asio::error_code ec, std::size_t bytes_transferred) {
        std::cout << "wrote " << bytes_transferred << "to client" << std::endl;
        if (ec)
        {
            // do some error
            std::cout << "Got error in sendToClient: " << ec.message() << std::endl;
        }
        else
        {
            self->recvFromServer();
        }
    };
    std::cout << "writing to server..." << std::endl;
    m_clientSock.async_write_some(asio::buffer(serverBuffer, bytes), lambda);
}

void Socks4::sendToServer(std::size_t bytes)
{
    auto lambda = [self = shared_from_this()](asio::error_code ec, std::size_t bytes_transferred) {
        std::cout << "wrote " << bytes_transferred << "to server" << std::endl;
        if (ec)
        {
            // do some error
            std::cout << "Got error in sendToServer: " << ec.message() << std::endl;
        }
        else
        {
            self->recvFromClient();
        }
    };
    std::cout << "writing to server..." << std::endl;
    m_serverSock.async_write_some(asio::buffer(clientBuffer, bytes), lambda);
}
}  // namespace socks
