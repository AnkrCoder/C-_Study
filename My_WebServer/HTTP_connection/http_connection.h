#ifndef HTTP_CONNECTION_H
#define HTTP_CONNECTION_H

#pragma once
#include "../Epoll_Reactor/Epoll_Reactor.h"
#include <string>
#include <map>
#include <functional>

class TcpConnection;

class HTTPConnection
{
public:
    explicit HTTPConnection(TcpConnection &conn, std::string root_dir);
    void handle_input(std::string &input_buffer);

private:
    void handle_get();             // 处理 GET 请求
    void handle_head();            // 处理 HEAD 请求
    void handle_post();            // 处理 POST 请求
    void handle_not_implemented(); // 处理未实现的请求

    bool parse_request(const std::string &buffer);              // 解析请求
    void prepare_response();                                    // 准备响应
    void send_response(int status, const std::string &content); // 发送错误响应

    std::string get_mime_type(const std::string &path) const; // 获取文件类型
    bool read_file_content(const std::string &path, std::string &content) const;

    TcpConnection &conn_; // TCP 连接

    std::string method_;
    std::string uri_;
    std::string version_;
    std::map<std::string, std::string> headers_;

    bool keep_alive_ = false;    // 长连接标识
    const std::string root_dir_; // 静态文件根目录

    static constexpr int HTTP_OK = 200;
    static constexpr int HTTP_BAD_REQUEST = 400;
    static constexpr int HTTP_FORBIDDEN = 403;
    static constexpr int HTTP_NOT_FOUND = 404;
    static constexpr int HTTP_METHOD_NOT_ALLOWED = 405;
    static constexpr int HTTP_INTERNAL_ERROR = 500;
    static constexpr int HTTP_NOT_IMPLEMENTED = 501;
};

#endif