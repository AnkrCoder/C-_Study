#include "Epoll_Reactor.h"
#include "../HTTP_connection/http_connection.h"
#include <boost/asio.hpp>
#include <iostream>

// Epoll Reactor class implementation
EpollReactor::EpollReactor() {
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        throw std::system_error(errno, std::generic_category(), "epoll_create1");
    }
    std::cout << "EpollReactor created with epoll_fd: " << epoll_fd_ << std::endl;
}

EpollReactor::~EpollReactor() {
    if (epoll_fd_ >= 0) close(epoll_fd_);
    std::cout << "EpollReactor destroyed" << std::endl;
}

void EpollReactor::add_fd(int fd, uint32_t events, EventCallback cb) {
    epoll_event ev{};
    ev.events = events;
    ev.data.ptr = new EventCallback(std::move(cb));

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
        delete static_cast<EventCallback*>(ev.data.ptr);
        throw std::system_error(errno, std::generic_category(), "epoll_ctl add");
    }
    std::cout << "Added fd: " << fd << " to epoll" << std::endl;
}

void EpollReactor::modify_fd(int fd, uint32_t events) {
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == -1) {
        throw std::system_error(errno, std::generic_category(), "epoll_ctl mod");
    }
    std::cout << "Modified fd: " << fd << " in epoll" << std::endl;
}

void EpollReactor::remove_fd(int fd) {
    std::cout << "Removing fd: " << fd << " from epoll" << std::endl;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        std::cerr << "Failed to remove fd: " << fd << " from epoll" << std::endl;
        throw std::system_error(errno, std::generic_category(), "epoll_ctl del");
    }
    std::cout << "Removed fd: " << fd << " from epoll" << std::endl;
}

void EpollReactor::run(int max_events, int timeout_ms) {
    std::vector<epoll_event> events(max_events);

    while (true) {
        int n = epoll_wait(epoll_fd_, events.data(), max_events, timeout_ms);
        if (n == -1) {
            if (errno == EINTR) continue;
            std::cerr << "epoll_wait error: " << std::strerror(errno) << std::endl;
            throw std::system_error(errno, std::generic_category(), "epoll_wait");
        }

        for (int i = 0; i < n; ++i) {
            auto* cb = static_cast<EventCallback*>(events[i].data.ptr);
            try {
                (*cb)(events[i].events);
            }
            catch (const std::exception& e) {
                std::cerr << "Event callback error: " << e.what() << " for fd: " << events[i].data.fd << std::endl;
            }
            delete cb;
        }
    }
}

// TCP Connection class implementation
std::shared_ptr<TcpConnection> TcpConnection::create(int fd, EpollReactor& reactor) {
    return std::shared_ptr<TcpConnection>(new TcpConnection(fd, reactor));
}

TcpConnection::TcpConnection(int fd, EpollReactor& reactor)
    : fd_(fd), reactor_(reactor) {
    set_nonblocking(fd_);
    std::cout << "TcpConnection created with fd: " << fd_ << std::endl;
}

TcpConnection::~TcpConnection() {
    if (fd_ >= 0) close(fd_);
    std::cout << "TcpConnection destroyed with fd: " << fd_ << std::endl;
}

void TcpConnection::start() {
    reactor_.add_fd(fd_, EPOLLIN | EPOLLET,
        [self = shared_from_this()](uint32_t events) {
            self->handle_event(events);
        });
    std::cout << "TcpConnection started with fd: " << fd_ << std::endl;
}

void TcpConnection::set_read_callback(ReadCallback cb) {
    read_cb_ = std::move(cb);
}

void TcpConnection::send(const std::string& data) {
    output_buffer_ += data;
    if (!writing_) {
        do_write();
    }
}

void TcpConnection::handle_event(uint32_t events) {
    std::cout << "Handling event for fd: " << fd_ << " with events: " << events << std::endl;
    try {
        std::cout << "Event type: " << events << std::endl;
        if (events & EPOLLIN) do_read();
        if (events & EPOLLOUT) do_write();
        if (events & (EPOLLERR | EPOLLHUP)) handle_close();
    }
    catch (...) {
        handle_close();
    }
}

void TcpConnection::do_read() {
    std::cout << "Reading from fd: " << fd_ << std::endl;
    char buf[4096];
    ssize_t n;

    while ((n = read(fd_, buf, sizeof(buf))) > 0) {
        std::cout << "Read " << n << " bytes from fd: " << fd_ << std::endl;
        input_buffer_.append(buf, n);
    }

    if (n == 0) {
        handle_close();
    }
    else if (n == -1 && errno != EAGAIN) {
        throw std::system_error(errno, std::generic_category(), "read");
    }

    if (read_cb_) {
        std::cout << "Invoking read callback for fd: " << fd_ << std::endl;
        read_cb_(input_buffer_);
        input_buffer_.clear();
    }
}

void TcpConnection::do_write() {
    writing_ = true;
    ssize_t n = write(fd_, output_buffer_.data(), output_buffer_.size());

    if (n > 0) {
        output_buffer_.erase(0, n);
        if (output_buffer_.empty()) {
            reactor_.modify_fd(fd_, EPOLLIN | EPOLLET);
            writing_ = false;
        }
    }
    else if (n == -1 && errno != EAGAIN) {
        throw std::system_error(errno, std::generic_category(), "write");
    }
}

void TcpConnection::handle_close() {
    std::cout << "Handling close for fd: " << fd_ << std::endl;
    reactor_.remove_fd(fd_);
    if (fd_ >= 0) {
        std::cout << "Closing fd: " << fd_ << std::endl;
        close(fd_);
        }
    fd_ = -1;
    std::cout << "TcpConnection closed with fd: " << fd_ << std::endl;
}

void TcpConnection::set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        throw std::system_error(errno, std::generic_category(), "fcntl F_GETFL");
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::system_error(errno, std::generic_category(), "fcntl F_SETFL");
    }
}

// TCP Acceptor class implementation
TcpAcceptor::TcpAcceptor(EpollReactor& reactor, uint16_t port)
    : reactor_(reactor) {
    listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listen_fd_ == -1) {
        throw std::system_error(errno, std::generic_category(), "socket");
    }

    int opt = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
        close(listen_fd_);
        throw std::system_error(errno, std::generic_category(), "setsockopt");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_fd_, (sockaddr*)&addr, sizeof(addr)) == -1) {
        close(listen_fd_);
        throw std::system_error(errno, std::generic_category(), "bind");
    }

    if (listen(listen_fd_, SOMAXCONN) == -1) {
        close(listen_fd_);
        throw std::system_error(errno, std::generic_category(), "listen");
    }

    reactor_.add_fd(listen_fd_, EPOLLIN | EPOLLET,
        [this](uint32_t events) { handle_accept(); });
    std::cout << "TcpAcceptor listening on port: " << port << std::endl;
}

TcpAcceptor::~TcpAcceptor() {
    if (listen_fd_ >= 0) close(listen_fd_);
    std::cout << "TcpAcceptor destroyed" << std::endl;
}

void TcpAcceptor::set_new_connection_callback(NewConnectionCallback cb) {
    new_conn_cb_ = std::move(cb);
}

void TcpAcceptor::handle_accept() {
    sockaddr_in client_addr{};
    socklen_t addrlen = sizeof(client_addr);

    while (true) {
        int conn_fd = accept4(listen_fd_, (sockaddr*)&client_addr, &addrlen, SOCK_NONBLOCK);
        if (conn_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            std::cerr << "accept4 error: " << std::strerror(errno) << std::endl;
            throw std::system_error(errno, std::generic_category(), "accept4");
        }

        std::cout << "New connection accepted with fd: " << conn_fd << std::endl;
        if (conn_fd < 0) {
            std::cerr << "Invalid file descriptor: " << conn_fd << std::endl;
            continue;
        }
        std::cout << "New connection accepted with fd: " << conn_fd << std::endl;
        if (new_conn_cb_) {
            std::cout << "Invoking new connection callback for fd: " << conn_fd << std::endl;
            new_conn_cb_(conn_fd);
        }
        std::cout << "Accepted new connection with fd: " << conn_fd << std::endl;
    }
}
