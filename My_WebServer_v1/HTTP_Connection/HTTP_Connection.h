#ifndef HTTP_CONNECTION_H
#define HTTP_CONNECTION_H

#pragma once
#include "../Epoll_Reactor/Epoll_Reactor.h"
#include <string>
#include <map>
#include <functional>

class HTTPConnection
{
public:
    explicit HTTPConnection(TcpConnection &conn, std::string root_dir);
    void handle_input(std::string &input_buffer);

private:
    void handle_get();  // 处理GET请求
    void handle_head(); // 处理HEAD请求
    void handle_post(); // 处理POST请求

    bool parse_request(const std::string &buffer);              // 解析请求
    void prepare_response();                                    // 准备响应
    void send_response(int status, const std::string &content); // 错误处理
    std::string get_mime_type(const std::string &path) const;
    bool read_file_content(const std::string &path, std::string &content) const; // 读取文件内容

    TcpConnection &conn_; // TCP连接

    std::string method_;                         // 请求方法
    std::string uri_;                            // 请求URI
    std::string version_;                        // HTTP版本
    std::map<std::string, std::string> headers_; // 请求头部集合

    bool keep_alive_ = false;    // 长连接标志
    const std::string root_dir_; // 根目录

    static constexpr int HTTP_OK = 200;
    static constexpr int HTTP_BAD_REQUEST = 400;
    static constexpr int HTTP_FORBIDDEN = 403;
    static constexpr int HTTP_NOT_FOUND = 404;
    static constexpr int HTTP_METHOD_NOT_ALLOWED = 405;
    static constexpr int HTTP_INTERNAL_ERROR = 500;
    static constexpr int HTTP_NOT_IMPLEMENTED = 501;
};

#endif