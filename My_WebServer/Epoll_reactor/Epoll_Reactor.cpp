#include "Epoll_Reactor.h"
#include "../Logger/Logger.h"

EpollReactor::EpollReactor()
{
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1)
    {
        throw std::system_error(errno, std::generic_category(), "epoll_create1");
    }
    Logger::get_instance().log(Logger::INFO,
                               "Epoll instance created (fd=" + std::to_string(epoll_fd_) + ")");
}

EpollReactor::~EpollReactor()
{
    if (epoll_fd_ >= 0)
    {
        std::cout << "Closing epoll instance fd=" << epoll_fd_ << std::endl;
        close(epoll_fd_);
    }
}

void EpollReactor::add_fd(int fd, uint32_t events, EventCallback cb)
{
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;

    callbacks_.emplace(fd, std::move(cb)); // 将fd和回调函数cb加入callbacks_

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1)
    {
        Logger::get_instance().log(Logger::ERROR, "epoll_ctl add failed for fd=" + std::to_string(fd) + ": " + std::to_string(errno));
        // throw std::system_error(errno, std::generic_category(), "epoll_ctl add");
    }
}

void EpollReactor::modify_fd(int fd, uint32_t events)
{
    if (fd < 0 || epoll_fd_ < 0)
    {
        Logger::get_instance().log(Logger::WARNING,
                                   "Attempt to modify invalid fd=" + std::to_string(fd) +
                                       " (epoll_fd= " + std::to_string(epoll_fd_) + ")");
        return;
    }

    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "epoll_ctl mod");
    }
}

void EpollReactor::remove_fd(int fd)
{

    if (fd < 0 || epoll_fd_ < 0)
    {
        std::cerr << "WARN: Attempt to remove invalid fd=" << fd
                  << " (epoll_fd=" << epoll_fd_ << ")\n";
        return;
    }

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1)
    {
        if (errno == EBADF) // 不是一个有效的文件描述符
        {
            std::cerr << "CRITICAL: Epoll instance(" << epoll_fd_
                      << ") or FD=" << fd << " is broken\n";
        }
        else
        {
            std::cerr << "ERROR: epoll_ctl del failed for fd=" << fd
                      << " (" << errno << ")\n";
        }

        throw std::system_error(errno, std::generic_category(), "epoll_ctl del");
    }
    callbacks_.erase(fd); // 从callbacks_中删除fd
}

void EpollReactor::run(int max_events, int timeout_ms)
{
    std::vector<epoll_event> events(max_events);

    while (true)
    {
        int n = epoll_wait(epoll_fd_, events.data(), max_events, timeout_ms);
        if (n == -1 && errno != EINTR)
        {
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

// 工厂函数
std::shared_ptr<TcpConnection> TcpConnection::create(int fd, EpollReactor &reactor)
{
    return std::shared_ptr<TcpConnection>(new TcpConnection(fd, reactor));
}

TcpConnection::TcpConnection(int fd, EpollReactor &reactor)
    : fd_(fd), reactor_(reactor)
{
    set_nonblocking(fd_);
}

TcpConnection::~TcpConnection()
{
    if (fd_ >= 0)
        close(fd_);
}

void TcpConnection::start()
{
    reactor_.add_fd(fd_, EPOLLIN | EPOLLET,
                    [self = shared_from_this()](uint32_t events)
                    {
                        self->handle_event(events);
                    });
}

void TcpConnection::set_read_callback(ReadCallback cb)
{
    read_cb_ = std::move(cb);   // 设置读回调函数
}

void TcpConnection::send(const std::string &data)
{
    output_buffer_ += data;
    if (!writing_)
    {
        reactor_.modify_fd(fd_, EPOLLIN | EPOLLOUT | EPOLLET);
    }
}

void TcpConnection::handle_event(uint32_t events)
{
    if (events & EPOLLIN)
        do_read();
    if (events & EPOLLOUT)
        do_write();
    if (events & (EPOLLERR | EPOLLHUP))
        handle_close();
}

void TcpConnection::do_read()
{
    char buf[4096];
    ssize_t n;

    while ((n = read(fd_, buf, sizeof(buf))) > 0)
    {
        input_buffer_.append(buf, n);
    }

    if (n == 0)
    {
        handle_close();
    }
    else if (n == -1 && errno != EAGAIN)
    {
        handle_close();
    }

    if (read_cb_)
    {
        read_cb_(input_buffer_);
        input_buffer_.clear();
    }
}

void TcpConnection::do_write()
{
    writing_ = true;
    ssize_t n = write(fd_, output_buffer_.data(), output_buffer_.size());

    if (n > 0)
    {
        output_buffer_.erase(0, n);
        if (output_buffer_.empty())
        {
            // std::cout << "do_write keep_alive_:" << keep_alive_ << std::endl;
            if (keep_alive_)
            {
                reactor_.modify_fd(fd_, EPOLLIN | EPOLLET);
            }
            else
            {
                handle_close();
            }
        }
    }
    else if (n == -1 && errno != EAGAIN)
    {
        handle_close();
    }
    writing_ = false;
}

void TcpConnection::handle_close()
{
    if (fd_ == -1)
        return;

    // std::cout << "BEGIN close fd=" << fd_ << "\n";

    reactor_.remove_fd(fd_);
    shutdown(fd_, SHUT_RDWR);
    close(fd_);

    fd_ = -1;
}

void TcpConnection::set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void TcpConnection::set_keep_alive(bool keep_alive)
{
    keep_alive_ = keep_alive;
}

TcpAcceptor::TcpAcceptor(EpollReactor &reactor, uint16_t port)
    : reactor_(reactor)
{
    listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listen_fd_ == -1)
    {
        throw std::system_error(errno, std::generic_category(), "socket");
    }

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

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
}

TcpAcceptor::~TcpAcceptor()
{
    if (listen_fd_ >= 0)
        close(listen_fd_);
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
            Logger::get_instance().log(Logger::ERROR,
                                       "Accept error: " + errno);
            throw std::system_error(errno, std::generic_category(), "accept4");
        }

        if (new_conn_cb_)
        {
            new_conn_cb_(conn_fd);
        }
    }
}
