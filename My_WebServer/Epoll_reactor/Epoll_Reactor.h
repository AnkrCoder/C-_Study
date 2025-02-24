#ifndef EPOLL_REACTOR_H
#define EPOLL_REACTOR_H

#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory>
#include <functional>
#include <unordered_map>
#include <system_error>
#include <string>
#include <iostream>
#include <vector>

class EpollReactor
{
public:
    using EventCallback = std::function<void(uint32_t events)>;

    EpollReactor();
    ~EpollReactor();

    EpollReactor(const EpollReactor &) = delete;
    EpollReactor &operator=(const EpollReactor &) = delete;

    void add_fd(int fd, uint32_t events, EventCallback cb);
    void modify_fd(int fd, uint32_t events);
    void remove_fd(int fd);
    void run(int max_events = 64, int timeout_ms = -1);

private:
    std::unordered_map<int, EventCallback> callbacks_;
    int epoll_fd_ = -1;
};

class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
public:
    using ReadCallback = std::function<void(std::string &)>;

    static std::shared_ptr<TcpConnection> create(int fd, EpollReactor &reactor);
    ~TcpConnection();

    TcpConnection(const TcpConnection &) = delete;
    TcpConnection &operator=(const TcpConnection &) = delete;

    void start();
    void set_read_callback(ReadCallback cb);
    void send(const std::string &data);
    void handle_close();
    void set_keep_alive(bool keep_alive);

private:
    TcpConnection(int fd, EpollReactor &reactor);

    void handle_event(uint32_t events);
    void do_read();
    void do_write();
    static void set_nonblocking(int fd);

    int fd_ = -1;
    EpollReactor &reactor_;
    std::string input_buffer_;
    std::string output_buffer_;
    bool writing_ = false;
    bool keep_alive_ = false;
    ReadCallback read_cb_;
};

class TcpAcceptor
{
public:
    using NewConnectionCallback = std::function<void(int fd)>;

    TcpAcceptor(EpollReactor &reactor, uint16_t port);
    ~TcpAcceptor();

    TcpAcceptor(const TcpAcceptor &) = delete;
    TcpAcceptor &operator=(const TcpAcceptor &) = delete;

    void set_new_connection_callback(NewConnectionCallback cb);

private:
    void handle_accept();

    int listen_fd_ = -1;
    EpollReactor &reactor_;
    NewConnectionCallback new_conn_cb_;
};

#endif