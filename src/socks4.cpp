#include "socks4.hpp"

using namespace socks;

Socks4::Socks4(asio::io_context& io, tcp::socket newsock)
    : m_clientSock(std::move(newsock)), m_io(io), m_serverSock(io)
// , clientBuffer(4 * 1048)
// , serverBuffer(4 * 1048)
{}

void Socks4::process(DataDirection from)
{

    // Guard clauses, if we are not actually in a state where we will touch a particular buffer we
    // want to just continoue to read
    //  From server guard:
    if (from == DataDirection::fromServer
        && !(m_processState == ProcessStates::passThrough
             || m_processState == ProcessStates::finished))
    {
        recvFromServer();
        return;
    }

    // Big switch state for processing all the data going throught the system
    switch (m_processState)
    {
        case ProcessStates::passThrough:
            // Handle normal data going through the system in established state.
            // That means passing data going client->server and server->client
            processPassThrough(from);
            return;
        case ProcessStates::finished: finish(); return;
        case ProcessStates::waitingForHandshake: {
            auto res = processHello(clientBuffer, m_handShakeMsg);
            if (res == ProcessStates::waitingForHandshake)
            {
                // We are not done with waiting for handshake, so we will break switch and then
                // recieve some more
                recvFromClient();
            }
            else
            {
                // If we are done with waiting for handshake we want to fall through
                m_processState = res;
                process(from);
                return;
            }
        }

        case ProcessStates::waitingForUserId: {
            auto res = processUserId(clientBuffer, m_userId);
            if (res == ProcessStates::waitingForUserId)
            {
                recvFromClient();
            }
            else
            {
                m_processState = res;
                process(from);
                return;
            }
        }

        case ProcessStates::waitingForConnect: {
            auto res = processConnect(m_serverSock, m_handShakeMsg);
            if (res == ProcessStates::waitingForConnect)
            {
                std::cout << "the impossible connect" << std::endl;
                std::terminate();
            }
            else
            {
                m_processState = res;
                process(from);
                return;
            }
        }
        case ProcessStates::sendHandshake: {
            auto res = processSendHandshake(m_clientSock, m_handShakeMsg);
            if (res == ProcessStates::sendHandshake)
            {
                std::cout << "the impossible send hansshake" << std::endl;
                std::terminate();
            }
            else
            {
                m_processState = res;
                process(from);
                return;
            }
        }
        case ProcessStates::initPassThrough: {
            // Start server recv and set state passthrough
            m_processState = ProcessStates::passThrough;
            recvFromServer();
            process(from);
            return;
        }
        default: break;
    }
}

ProcessStates Socks4::processHello(std::vector<uint8_t>& buffer, HandshakeMsg& handShakeMsg)
{
    std::cout << "processing Hello" << std::endl;
    // modifes handShake paramtere with new data from the buffer sent inn. Buffer will also be
    // modified by removing data read from the buffer

    //  Check that the data in the buffer is big enough to be moved into handshakemsg
    if (buffer.size() < sizeof(handShakeMsg))
    {
        // We dont have enough data yet, continue to read
        return ProcessStates::waitingForHandshake;
    }
    std::cout << "got enough data for handshake: " << buffer.size() << " bytes" << std::endl;
    // Memcopy buffer data over to handshakemsg

    handShakeMsg = parseHandShake(buffer);
    // now that we have gotten the handshake, need to reduce the size of the buffer
    buffer.erase(buffer.begin(), buffer.begin() + sizeof(handShakeMsg));


    if (handShakeMsg.version != 0x04)
    {
        std::cout << "Wrong SOCKS version, got " << handShakeMsg.version << std::endl;
        return ProcessStates::sendHandshake;
    }

    if (handShakeMsg.cmd != 0x01)
    {
        std::cout << "Not the correct command, got " << handShakeMsg.cmd << std::endl;
        return ProcessStates::sendHandshake;
    }

    std::cout << "Got a message: " << std::endl
              << "msg version: " << static_cast<int>(handShakeMsg.version) << std::endl
              << "msg cmd: " << static_cast<int>(handShakeMsg.cmd) << std::endl
              << "msg port: " << static_cast<int>(handShakeMsg.port) << std::endl
              << "msg dst: "
              //   << asio::ip::address_v4(self->m_handShakeMsg.destination).to_string()
              << asio::ip::address_v4(handShakeMsg.destination).to_string() << std::endl;

    return ProcessStates::waitingForUserId;
}

HandshakeMsg Socks4::parseHandShake(const std::vector<uint8_t>& buffer)
{
    HandshakeMsg msg{};
    std::memcpy(&msg, buffer.data(), sizeof(msg));

    // Transform from network byte order to "normal byte order"
    std::uint32_t number = msg.destination;
    int last = (number >> (8 * 0)) & 0xff;
    int third = (number >> (8 * 1)) & 0xff;
    int scnd = (number >> (8 * 2)) & 0xff;
    int first = (number >> (8 * 3)) & 0xff;
    std::uint32_t converted = first | (scnd << 8) | (third << 16) | (last << 24);
    msg.destination = converted;
    // auto pre = reinterpret_cast<std::uint8_t[]>(self->m_handShakeMsg.destination);

    // converting port:
    std::uint16_t nport = msg.port;
    int plast = (nport >> (8 * 0)) & 0xff;
    int pthird = (nport >> (8 * 1)) & 0xff;
    std::uint16_t convertedPort = pthird | (plast << 8);
    msg.port = convertedPort;

    return msg;
}

ProcessStates Socks4::processUserId(std::vector<uint8_t>& buffer, std::string& uid)
{
    std::cout << "processing UserId" << std::endl;
    int terminatorIndex = findUserIdTerminator(buffer);
    if (terminatorIndex >= 0)
    {
        std::cout << "found user id:" << std::endl;
        // memcpy would probably also work
        std::string userId{};
        std::for_each(buffer.begin(),
                      buffer.begin() + terminatorIndex,
                      [&userId](std::uint8_t data) { userId.push_back(data); });
        uid = userId;

        // Remove read bytes from the buffer
        buffer.erase(buffer.begin(), buffer.begin() + terminatorIndex + 1);
        return ProcessStates::waitingForConnect;
    }
    else
    {
        return ProcessStates::waitingForUserId;
    }
}

ProcessStates Socks4::processConnect(tcp::socket& sock, HandshakeMsg& handShakeMsg)
{
    std::cout << "Processing connect" << std::endl;
    asio::error_code ec{};
    auto endpoint =
        tcp::endpoint(asio::ip::address_v4(handShakeMsg.destination), handShakeMsg.port);
    std::cout << "Connecting..." << std::endl;

    sock.connect(endpoint, ec);

    if (ec)
    {
        std::cout << "Could not connect!" << std::endl;
        handShakeMsg.cmd = 0x5B;
    }
    else
    {
        std::cout << "connected to server!" << std::endl;
        handShakeMsg.version = 0x00;
        handShakeMsg.cmd = 0x5A;
        // Now we are connected and we want to start rading from the server aswell:
    }

    return ProcessStates::sendHandshake;
}

ProcessStates Socks4::processSendHandshake(tcp::socket& sock, HandshakeMsg msg)
{
    std::cout << "Processing SendHandshake" << std::endl;
    std::cout << "Sending handshake to client..." << std::endl;
    asio::error_code ec{};
    sock.write_some(asio::buffer(&msg, sizeof(msg)), ec);

    if (msg.cmd == 0x5A)
    {
        // 5A means that everything went fine
        std::cout << "going to passThrough" << std::endl;
        return ProcessStates::initPassThrough;
    }
    else
    {
        // everything is not fine, stop
        return ProcessStates::finished;
    }
}

void Socks4::processPassThrough(DataDirection from)
{
    // Big switch on if we should read from somewhere or send to somewhere
    switch (from)
    {
        case DataDirection::fromClient:
            // We just got some data from the client, then we want to sent to the server
            sendToServer();
            break;
        case DataDirection::fromServer:
            // Just got data from server, send it to client
            sendToClient();
            break;
        case DataDirection::toClient:
            // Just sent some data to client, now read from server again
            recvFromServer();
            break;
        case DataDirection::toServer:
            // Just sent some data to server, now read from server client
            recvFromClient();
            break;
        default: break;
    }
}

void Socks4::finish() {}

void Socks4::recvHello()
{
    // Here we read out what we need for the hello package, then connect and if that works we send
    // hello back and start listening to everything.

    // recieve the hello packet(no user id, will read that later)
    auto lambda = [self = shared_from_this()](asio::error_code ec, std::size_t bytes_read) {
        std::cout << "Read: " << bytes_read << " bytes" << std::endl;

        if (ec)
        {
            std::cout << "Failed handshake..." << std::endl;
            return;
        }
        std::cout << "buffer length: " << self->clientBuffer.size() << std::endl;

        // resize buffer to correct length
        self->m_helloMsgLengh += bytes_read;
        self->clientBuffer.resize(self->m_helloMsgLengh);
        std::cout << "buffer length: " << self->clientBuffer.size() << std::endl;

        // self->processHello();  // Check to see if we got enough data
    };

    // Add 4k bytes to vector:
    clientBuffer.resize(clientBuffer.size() + 4000);
    std::cout << "buffer length: " << clientBuffer.size() << std::endl;
    m_clientSock.async_read_some(asio::buffer(clientBuffer), lambda);
}

int Socks4::findUserIdTerminator(const std::vector<uint8_t>& buffer)
{
    // Checks client buffer for nullterminator, returns the index if it finds one, -1 if nothing is
    // found
    for (auto it = buffer.begin(); it != clientBuffer.end(); ++it)
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
        std::cerr << "There was an error in sendhello" << m_handShakeMsg.cmd << std::endl;
        return;
    }
}

void Socks4::recvFromClient()
{
    auto lambda = [self = shared_from_this()](asio::error_code ec, std::size_t bytes_transferred) {
        self->m_clientBufferLength += bytes_transferred;
        self->clientBuffer.resize(self->m_clientBufferLength);
        if (ec)
        {
            // do some error stuff...
            std::cerr << "got error in recvFromClient: " << ec.message() << std::endl;
            if (bytes_transferred > 0)  // If we read some bytes even tho there was an error we
                                        // send that data forward non the less
            {
                self->process(DataDirection::fromClient);
            }
            return;
        }
        self->process(DataDirection::fromClient);
    };
    m_clientBufferLength = clientBuffer.size();
    clientBuffer.resize(clientBuffer.size() + m_newBlockSize);
    auto buffer = asio::buffer(clientBuffer.data() + m_clientBufferLength, m_newBlockSize);
    m_clientSock.async_read_some(buffer, lambda);
}

void Socks4::recvFromServer()
{
    // Want som form of timeout setup
    // setClientTimeout(); or something
    auto lambda = [self = shared_from_this()](asio::error_code ec, std::size_t bytes_transferred) {
        self->m_serverBufferLength += bytes_transferred;
        self->serverBuffer.resize(self->m_serverBufferLength);
        if (ec)
        {
            // do some error stuff...
            std::cerr << "Got error recvFromServer: " << ec.message() << std::endl;
            if (bytes_transferred > 0)  // If we read some bytes even tho there was an error we send
                                        //  that data forward non the less
            {
                self->process(DataDirection::fromServer);
            }
            return;
        }
        self->process(DataDirection::fromServer);
    };
    m_serverBufferLength = serverBuffer.size();
    serverBuffer.resize(serverBuffer.size() + m_newBlockSize);
    auto buffer = asio::buffer(serverBuffer.data() + m_serverBufferLength, m_newBlockSize);
    m_serverSock.async_read_some(buffer, lambda);
}

void Socks4::sendToClient()
{
    auto lambda = [self = shared_from_this()](asio::error_code ec, std::size_t bytes_transferred) {
        if (ec)
        {
            // do some error
            std::cerr << "Got error in sendToClient: " << ec.message() << std::endl;
            return;
        }
        else
        {
            // Resize buffer
            self->serverBuffer.erase(self->serverBuffer.begin(),
                                     self->serverBuffer.begin() + bytes_transferred);
            self->m_serverBufferLength = self->serverBuffer.size();
            self->process(DataDirection::toClient);
        }
    };
    // std::cout << "writing to server..." << std::endl;
    m_clientSock.async_write_some(asio::buffer(serverBuffer, serverBuffer.size()), lambda);
}

void Socks4::sendToServer()
{
    auto lambda = [self = shared_from_this()](asio::error_code ec, std::size_t bytes_transferred) {
        if (ec)
        {
            // do some error
            std::cerr << "Got error in sendToServer: " << ec.message() << std::endl;
            return;
        }
        else
        {

            self->clientBuffer.erase(self->clientBuffer.begin(),
                                     self->clientBuffer.begin() + bytes_transferred);
            self->m_clientBufferLength = self->clientBuffer.size();
            self->process(DataDirection::toServer);
        }
    };
    m_serverSock.async_write_some(asio::buffer(clientBuffer, clientBuffer.size()), lambda);
}
