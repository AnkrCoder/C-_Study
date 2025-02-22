#include "Epoll_reactor/Epoll_Reactor.h"
#include "HTTP_connection/http_connection.h"
#include <boost/asio.hpp>
#include <iostream>
#include <unordered_set>

std::unordered_set<std::shared_ptr<HttpConnection>> active_connections;
boost::asio::io_context io_context; // 全局唯一io_context

int main() {
    try {
        EpollReactor reactor;
        TcpAcceptor acceptor(reactor, 8088);

        acceptor.set_new_connection_callback([&](int fd) {
            try {
                auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context);
                socket->assign(boost::asio::ip::tcp::v4(), fd);
                
                auto conn = std::make_shared<HttpConnection>(std::move(*socket));
                active_connections.insert(conn); // 维持生命周期
                
                conn->read_request();
                std::cout << "New connection fd: " << fd << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Connection setup failed: " << e.what() << std::endl;
                close(fd);
            }
        });

        reactor.run();
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
