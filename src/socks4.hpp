#pragma once

#include <memory>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <cstring>  //memcpy
#include <cstddef>  //std::byte

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

enum ProcessStates
{
    waitingForHandshake,
    waitingForUserId,
    waitingForConnect,
    sendHandshake,
    initPassThrough,
    passThrough,
    finished
};

enum DataDirection
{
    fromServer,
    fromClient,
    toServer,
    toClient
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
    size_t m_clientBufferLength{ 0 };
    std::vector<uint8_t> serverBuffer;
    size_t m_serverBufferLength{ 0 };
    HandshakeMsg m_handShakeMsg;
    std::string m_userId;
    std::uint8_t m_userIdSize;
    int maxIdLength{ 1000 };
    int m_helloMsgLengh{};
    ProcessStates m_processState{ ProcessStates::waitingForHandshake };
    int m_newBlockSize{ 4000 };

    // Provess stuff functions
    void process(DataDirection from);
    ProcessStates processHello(std::vector<uint8_t>& buffer, HandshakeMsg& handShakeMsg);
    ProcessStates processUserId(std::vector<uint8_t>& buffer, std::string& uid);
    ProcessStates processConnect(tcp::socket& soc, HandshakeMsg& handShakeMsg);
    ProcessStates processSendHandshake(tcp::socket& sock, const HandshakeMsg msg);
    void processPassThrough(DataDirection from);
    void finish();  // redundant?

    // Utils
    int findUserIdTerminator(const std::vector<uint8_t>& buffer);
    HandshakeMsg parseHandShake(const std::vector<uint8_t>& buffer);

    // read/write data func per socket, or one generic one with socket som parameter?
    // void readData(tcp::socket& socket, asio::buffer buffer);
    // void sendData(tcp::socket& socket, asio::buffer buffer);

  public:
    Socks4(asio::io_context& io, tcp::socket newSock);

    void recvFromClient();
    void recvFromServer();
    void sendToClient();
    void sendToServer();
    void recvHello();
    void recvUserId();
    void sendhello();
    void sendToClient(std::size_t bytes);
    void sendToServer(std::size_t bytes);
    void connectToServer();
};

}  // namespace socks
