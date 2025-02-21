#include "Epoll_reactor/Epoll_Reactor.h"
#include "HTTP_connection/http_connection.h"
#include <boost/asio.hpp>
#include <iostream>

int main() {
    try {
        EpollReactor reactor;
        TcpAcceptor acceptor(reactor, 8088);

        boost::asio::io_context ioc;
        acceptor.set_new_connection_callback([&ioc](int fd) {
            boost::asio::ip::tcp::socket socket(ioc);
            socket.assign(boost::asio::ip::tcp::v4(), fd);
            std::make_shared<HttpConnection>(std::move(socket))->start();
            std::cout << "New connection callback for fd: " << fd << std::endl;
        });

        ioc.run();
        reactor.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
