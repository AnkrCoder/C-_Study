#include "http_connection.h"

#include <fstream>
#include <iostream>

constexpr const char *DOCUMENT_ROOT = "root"; // 静态文件根目录

HttpConnection::HttpConnection(tcp::socket socket)
    : socket_(std::move(socket)) {}

void HttpConnection::read_request()
{
    http::read(socket_, buffer_, req_);
    handle_request(std::move(req_));
}

void HttpConnection::handle_request(http::request<http::string_body> &&req)
{
    try
    {
        switch (req.method())
        {
        case boost::beast::http::verb::get:
            std::cout << "handle_get started: " << std::endl;
            handle_get(req);
            break;
        case boost::beast::http::verb::post:
            std::cout << "handle_post started: " << std::endl;
            handle_post(req);
            break;
        default:
            // 返回405 Method Not Allowed
            res_.result(boost::beast::http::status::method_not_allowed);
            res_.version(req.version());
            res_.set(boost::beast::http::field::content_type, "text/plain");
            res_.body() = "HTTP Method Not Implemented";
            res_.prepare_payload();
        }
    }
    catch (const std::exception &e)
    {
        // 异常处理返回500错误
        res_.result(boost::beast::http::status::internal_server_error);
        res_.set(boost::beast::http::field::content_type, "text/plain");
        res_.body() = "Internal Server Error: " + std::string(e.what());
        res_.prepare_payload();
    }

    send_response(std::move(res_));
}

std::filesystem::path HttpConnection::resolve_path(const std::string &target)
{
    try
    {
        std::cout << "start resolve_path: " << "target: " << target << std::endl;
        // 基础路径拼接
        std::filesystem::path base_path(DOCUMENT_ROOT);
        std::cout << "base_path: " << base_path << std::endl;
        std::filesystem::path request_path = base_path / target.substr(1); // 去掉开头的/
        std::cout << "request_path: " << request_path << std::endl;

        // 简化安全检查
        if (!exists(request_path))
        {
            throw std::runtime_error("404 Not Found");
        }

        // 防止目录遍历的基本检查
        std::string request_str = request_path.string();
        if (request_str.find("..") != std::string::npos)
        {
            throw std::runtime_error("403 Forbidden");
        }

        // 如果是目录则查找index.html
        if (is_directory(request_path))
        {
            request_path /= "index.html";
            if (!exists(request_path))
            {
                throw std::runtime_error("404 No index");
            }
        }

        return request_path;
    }
    catch (...)
    {
        throw std::runtime_error("Invalid path resolution");
    }
}

void HttpConnection::send_response(http::response<http::string_body> &&res)
{
    beast::http::write(socket_, res);
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send);
}

// 优化文件读取逻辑
void HttpConnection::handle_get(
    const http::request<http::string_body> &req)
{
    std::cout << "handle_get operating: " << std::endl;
    try
    {
        auto path = resolve_path(req.target());

        std::ifstream file(path, std::ios::binary);
        if (!file)
            throw std::runtime_error("File open failed");

        // 文件读取
        std::string content(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());

        res_.result(http::status::ok);
        res_.version(req.version());
        res_.set(http::field::content_type, get_mime_type(path.extension()));
        res_.body() = std::move(content);
    }
    catch (const std::exception &e)
    {
        res_.version(req.version());
        res_.set(http::field::content_type, "text/html");
        if (e.what() == std::string("403 Forbidden"))
        {
            res_.result(http::status::forbidden);
            res_.body() = "<h1>403 Forbidden</h1>";
        }
        else
        {
            res_.result(http::status::not_found);
            res_.body() = "<h1>404 Not Found</h1>";
        }
    }
    res_.prepare_payload();
}

void HttpConnection::handle_post(const http::request<http::string_body> &req)
{
    std::cout << "handle_post operating: " << std::endl;
    res_.result(http::status::ok);
    res_.version(req.version());
    res_.set(http::field::content_type, "text/plain");
    res_.body() = "Post text\nReceived data: " + req.body();
    res_.prepare_payload();
}

// MIME类型映射辅助函数
std::string HttpConnection::get_mime_type(const std::string &ext)
{
    static const std::unordered_map<std::string, std::string> mime_types{
        {".html", "text/html"},
        {".htm", "text/html"},
        {".txt", "text/plain"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".ico", "image/x-icon"}};

    auto it = mime_types.find(ext);
    return it != mime_types.end() ? it->second : "application/octet-stream";
}