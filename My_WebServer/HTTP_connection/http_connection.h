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
    void handle_get();
    void handle_head();
    void handle_post();
    void handle_not_implemented();

    bool parse_request(const std::string &buffer);
    void prepare_response();
    void send_response(int status, const std::string &content);
    std::string get_mime_type(const std::string &path) const;
    bool read_file_content(const std::string &path, std::string &content) const;

    TcpConnection &conn_;

    std::string method_;
    std::string uri_;
    std::string version_;
    std::map<std::string, std::string> headers_;

    bool keep_alive_ = false;
    const std::string root_dir_;

    // 新增状态码常量
    static constexpr int HTTP_OK = 200;
    static constexpr int HTTP_BAD_REQUEST = 400;
    static constexpr int HTTP_FORBIDDEN = 403;
    static constexpr int HTTP_NOT_FOUND = 404;
    static constexpr int HTTP_METHOD_NOT_ALLOWED = 405;
    static constexpr int HTTP_INTERNAL_ERROR = 500;
    static constexpr int HTTP_NOT_IMPLEMENTED = 501;
};

#endif