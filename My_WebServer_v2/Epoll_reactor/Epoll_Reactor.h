#include <sys/epoll.h>
#include <unordered_map>
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

// Epoll Reactor class
class EpollReactor
{
public:
    using EventCallback = std::function<void(uint32_t events)>;

    EpollReactor();
    ~EpollReactor();

    // Disable copy
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


// TCP Acceptor class
class TcpAcceptor
{
public:
    using NewConnectionCallback = std::function<void(int fd)>;

    TcpAcceptor(EpollReactor &reactor, uint16_t port);
    ~TcpAcceptor();

    // Disable copy
    TcpAcceptor(const TcpAcceptor &) = delete;
    TcpAcceptor &operator=(const TcpAcceptor &) = delete;

    void set_new_connection_callback(NewConnectionCallback cb);

private:
    void handle_accept();

    int listen_fd_ = -1;
    EpollReactor &reactor_;
    NewConnectionCallback new_conn_cb_;
};
