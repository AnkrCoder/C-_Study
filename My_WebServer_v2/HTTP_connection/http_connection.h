#pragma once
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <filesystem>
#include <memory>

// 命名空间简化
namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class HttpConnection : public std::enable_shared_from_this<HttpConnection>
{
public:
    explicit HttpConnection(tcp::socket socket);
    void read_request();

private:
    void handle_request(http::request<http::string_body>&& req);
    void handle_get(const http::request<http::string_body>& req);
    void handle_post(const http::request<http::string_body>& req);
    void send_response(http::response<http::string_body>&& res);
    
    std::filesystem::path resolve_path(const std::string& target);
    std::string get_mime_type(const std::string& ext);

    tcp::socket socket_;
    beast::multi_buffer buffer_;
    http::request<http::string_body> req_;
    http::response<http::string_body> res_;
};
