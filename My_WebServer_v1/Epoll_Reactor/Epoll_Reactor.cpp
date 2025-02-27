#include "Epoll_Reactor.h"
#include "../Logger/Logger.h"
#include "../Master_Worker/master_worker.h"

EpollReactor::EpollReactor()
{
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1)
    {
        throw std::system_error(errno, std::generic_category(), "epoll_create1");
    }
    Logger::get_instance().log(Logger::INFO,
                               "Epoll instance created (epoll_fd_=" + std::to_string(epoll_fd_) + ")");
}

EpollReactor::~EpollReactor()
{
    if (epoll_fd_ >= 0)
    {
        Logger::get_instance().log(Logger::INFO,
                                   "Closing epoll instance (epoll_fd_=" + std::to_string(epoll_fd_) + ")");
        // std::cerr << "Closing epoll instance fd=" << epoll_fd_ << "\n";
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
        throw std::system_error(errno, std::generic_category(), "epoll_ctl add");
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
        Logger::get_instance().log(Logger::WARNING, "WARN: Attempt to remove invalid fd=" + std::to_string(fd) +
                                                        " (epoll_fd=" + std::to_string(epoll_fd_) + ")");

        // std::cerr << "WARN: Attempt to remove invalid fd=" << fd
        //           << " (epoll_fd=" << epoll_fd_ << ")\n";
        return;
    }

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1)
    {
        if (errno == EBADF) // 如果fd或epoll_fd_无效，则输出错误信息
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

    while (running_)
    {
        int n = epoll_wait(epoll_fd_, events.data(), max_events, timeout_ms);
        if (n == -1)
        {
            if (errno != EINTR)
            { // 如果不是被信号中断，抛出异常
                throw std::system_error(errno, std::generic_category(), "epoll_wait");
            }
            // 如果是被信号中断,则退出循环
            continue;
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

void EpollReactor::stop()
{
    if (running_)
    {
        running_ = false;
    }
}

// TCP连接处理器
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
    {
        shutdown(fd_, SHUT_RDWR);
        close(fd_);
    }
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
    read_cb_ = std::move(cb);
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
    Logger::get_instance().log(Logger::INFO,
                               "Worker " + std::to_string(getpid()) +
                                   " reading fd=" + std::to_string(fd_));

    char buf[8192];
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
    Logger::get_instance().log(Logger::INFO,
                               "Worker " + std::to_string(getpid()) +
                                   " writing fd=" + std::to_string(fd_));

    writing_ = true; // 标记写入状态，防止重入

    // 边缘触发模式必须尝试完全写入
    while (!output_buffer_.empty())
    {
        ssize_t n = ::write(fd_, output_buffer_.data(), output_buffer_.size());

        if (n > 0)
        {
            output_buffer_.erase(0, n); // 移除已发送数据
            continue;
        }

        if (n == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 内核缓冲区已满，需要等待下次可写事件
                reactor_.modify_fd(fd_, EPOLLIN | EPOLLOUT | EPOLLET);
            }
            else
            {
                handle_close(); // 发生实际错误，关闭连接
            }
            break; // 退出循环
        }
    }

    // 根据缓冲区状态调整监听事件
    if (output_buffer_.empty())
    {
        // 数据已全部发送，停止监听写事件
        reactor_.modify_fd(fd_, EPOLLIN | EPOLLET);

        // 非keep-alive连接需要立即关闭
        if (!keep_alive_)
        {
            handle_close();
        }
    }

    writing_ = false; // 重置写入状态
}

void TcpConnection::handle_close()
{
    if (fd_ == -1)
    {
        return;
    }

    Logger::get_instance().log(Logger::INFO, "Closing connection fd=" + std::to_string(fd_));

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

// TCP连接接收器
TcpAcceptor::TcpAcceptor(EpollReactor &reactor, uint16_t listen_fd)
    : reactor_(reactor), listen_fd_(listen_fd)
{
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
            break;
        }

        Logger::get_instance().log(Logger::INFO, "Accepted new connection fd=" + std::to_string(conn_fd) + " by worker " + std::to_string(getpid()));

        // 设置SO_LINGER选项，以优雅地关闭连接
        struct linger linger_opts = {1, 1};
        setsockopt(conn_fd, SOL_SOCKET, SO_LINGER, &linger_opts, sizeof(linger_opts));

        if (new_conn_cb_)
        {
            new_conn_cb_(conn_fd);
        }
    }
}
