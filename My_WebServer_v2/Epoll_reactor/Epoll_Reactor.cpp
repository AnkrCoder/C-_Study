#include "Epoll_Reactor.h"
#include "../HTTP_connection/http_connection.h"
#include <boost/asio.hpp>
#include <iostream>

// Epoll Reactor
EpollReactor::EpollReactor()
{
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1)
    {
        throw std::system_error(errno, std::generic_category(), "epoll_create1");
    }
    std::cout << "EpollReactor created with epoll_fd: " << epoll_fd_ << std::endl;
}

EpollReactor::~EpollReactor()
{
    if (epoll_fd_ >= 0)
        close(epoll_fd_);
    std::cout << "EpollReactor destroyed" << std::endl;
}

void EpollReactor::add_fd(int fd, uint32_t events, EventCallback cb)
{
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;

    callbacks_.emplace(fd, std::move(cb)); // 存储回调到map

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "epoll_ctl add");
    }
    std::cout << "Added fd: " << fd << " to epoll" << std::endl;
}

void EpollReactor::modify_fd(int fd, uint32_t events)
{
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "epoll_ctl mod");
    }
    std::cout << "Modified fd: " << fd << " in epoll" << std::endl;
}

void EpollReactor::remove_fd(int fd)
{
    std::cout << "Removing fd: " << fd << " from epoll" << std::endl;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1)
    {
        std::cerr << "Failed to remove fd: " << fd << " from epoll" << std::endl;
        throw std::system_error(errno, std::generic_category(), "epoll_ctl del");
    }
    std::cout << "Removed fd: " << fd << " from epoll" << std::endl;
}

void EpollReactor::run(int max_events, int timeout_ms)
{
    std::vector<epoll_event> events(max_events);

    while (true)
    {
        int n = epoll_wait(epoll_fd_, events.data(), max_events, timeout_ms);
        if (n == -1)
        {
            if (errno == EINTR)
                continue;
            std::cerr << "epoll_wait error: " << std::strerror(errno) << std::endl;
            throw std::system_error(errno, std::generic_category(), "epoll_wait");
        }

        for (int i = 0; i < n; ++i)
        {
            auto it = callbacks_.find(events[i].data.fd);
            if (it != callbacks_.end())
            {
                it->second(events[i].events);
            }
        }
    }
}

// TCP Acceptor
TcpAcceptor::TcpAcceptor(EpollReactor &reactor, uint16_t port)
    : reactor_(reactor)
{
    listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listen_fd_ == -1)
    {
        throw std::system_error(errno, std::generic_category(), "socket");
    }

    int opt = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == -1)
    {
        close(listen_fd_);
        throw std::system_error(errno, std::generic_category(), "setsockopt");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_fd_, (sockaddr *)&addr, sizeof(addr)) == -1)
    {
        close(listen_fd_);
        throw std::system_error(errno, std::generic_category(), "bind");
    }

    if (listen(listen_fd_, SOMAXCONN) == -1)
    {
        close(listen_fd_);
        throw std::system_error(errno, std::generic_category(), "listen");
    }

    reactor_.add_fd(listen_fd_, EPOLLIN | EPOLLET,
                    [this](uint32_t events)
                    { handle_accept(); });
    std::cout << "TcpAcceptor listening on port: " << port << std::endl;
}

TcpAcceptor::~TcpAcceptor()
{
    if (listen_fd_ >= 0)
        close(listen_fd_);
    std::cout << "TcpAcceptor destroyed" << std::endl;
}

void TcpAcceptor::set_new_connection_callback(NewConnectionCallback cb)
{
    new_conn_cb_ = std::move(cb);
}

void TcpAcceptor::handle_accept()
{
    sockaddr_in client_addr{};
    socklen_t addrlen = sizeof(client_addr);

    while (true)
    {
        int conn_fd = accept4(listen_fd_, (sockaddr *)&client_addr, &addrlen, SOCK_NONBLOCK);
        if (conn_fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            std::cerr << "accept4 error: " << std::strerror(errno) << std::endl;
            throw std::system_error(errno, std::generic_category(), "accept4");
        }
        if (conn_fd < 0)
        {
            continue;
        }
        if (new_conn_cb_)
        {
            new_conn_cb_(conn_fd);
        }
        std::cout << "Accepted new connection with fd: " << conn_fd << std::endl;
    }
}
