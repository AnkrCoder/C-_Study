#ifndef EPOLL_REACTOR_H
#define EPOLL_REACTOR_H

#include <sys/eventfd.h>
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
#include <atomic>

class ProcessMaster;

class EpollReactor
{
public:
    using EventCallback = std::function<void(uint32_t events)>; // 事件回调函数别名

    EpollReactor();
    ~EpollReactor();

    EpollReactor(const EpollReactor &) = delete;
    EpollReactor &operator=(const EpollReactor &) = delete;

    void add_fd(int fd, uint32_t events, EventCallback cb); // 添加fd到epoll监听
    void modify_fd(int fd, uint32_t events);                // 修改fd的监听事件
    void remove_fd(int fd);                                 // 移除fd
    void run(int max_events = 4096, int timeout_ms = -1);   // 开始事件循环
    void stop(); // 停止事件循环

private:
    std::unordered_map<int, EventCallback> callbacks_;  // 事件回调存储容器
    int epoll_fd_ = -1;
    std::atomic<bool> running_{true}; // 控制事件循环
};

class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
public:
    using ReadCallback = std::function<void(std::string &)>; // 读回调函数

    static std::shared_ptr<TcpConnection> create(int fd, EpollReactor &reactor);
    ~TcpConnection();

    TcpConnection(const TcpConnection &) = delete;
    TcpConnection &operator=(const TcpConnection &) = delete;

    void start();
    void set_read_callback(ReadCallback cb);
    void send(const std::string &data); // 设置响应数据，准备发送
    void handle_close();
    void set_keep_alive(bool keep_alive); // 设置是否保持连接

    int fd() const { return fd_; }

private:
    TcpConnection(int fd, EpollReactor &reactor);

    void handle_event(uint32_t events);
    void do_read();  // 读事件处理
    void do_write(); // 写事件处理
    static void set_nonblocking(int fd);

    int fd_ = -1;
    EpollReactor &reactor_;

    std::string input_buffer_;  // 输入缓冲区
    std::string output_buffer_; // 输出缓冲区

    bool writing_ = false;
    bool keep_alive_ = false;
    ReadCallback read_cb_;
};

class TcpAcceptor
{
public:
    using NewConnectionCallback = std::function<void(int fd)>; // 新连接回调函数

    TcpAcceptor(EpollReactor &reactor, uint16_t port);
    ~TcpAcceptor();

    TcpAcceptor(const TcpAcceptor &) = delete;
    TcpAcceptor &operator=(const TcpAcceptor &) = delete;

    void set_new_connection_callback(NewConnectionCallback cb);

private:
    void handle_accept();   // 处理新连接

    EpollReactor &reactor_;
    int listen_fd_ = -1;
    NewConnectionCallback new_conn_cb_;
};

#endif