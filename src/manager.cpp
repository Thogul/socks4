#include "manager.hpp"

using namespace socks;

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
        newSession->recvFromClient();  // start first callback

        self->startAccept();
        // [this, other = shared_from_this()](void) {
        //     startAccept();
        // }();  // Start accepting again, Prøve uten denne...
    };
    m_acceptor.async_accept(lambda);
    std::cout << "Accept setup!" << std::endl;
}
