#include "HTTP_Connection/HTTP_Connection.h"
#include "Logger/Logger.h"
#include <iostream>

int main()
{
    try
    {
        Logger::get_instance().init("logging", 1000);
        Logger::get_instance().log(Logger::INFO, "Server started successfully"); // test

        EpollReactor reactor;
        TcpAcceptor acceptor(reactor, 8080);

        acceptor.set_new_connection_callback([&reactor](int fd)
                                             {
            auto conn = TcpConnection::create(fd, reactor);
            auto http_conn = std::make_shared<HTTPConnection>(*conn, "./root");
            
            conn->set_read_callback([http_conn](std::string& buf) {
                http_conn->handle_input(buf);
            });
            
            conn->start(); });

        reactor.run();
    }
    catch (const std::exception &e)
    {
        Logger::get_instance().log(Logger::ERROR, e.what());
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
