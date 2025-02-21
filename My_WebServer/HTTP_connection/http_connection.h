#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <memory>
#include <string>
#include <vector>

class HttpConnection : public std::enable_shared_from_this<HttpConnection> {
public:
    HttpConnection(boost::asio::ip::tcp::socket socket);
    void start();

private:
    void do_read();
    void handle_request(boost::beast::http::request<boost::beast::http::string_body>&& req);
    void send_response(boost::beast::http::response<boost::beast::http::string_body>&& res);

    boost::asio::ip::tcp::socket socket_;
    boost::beast::flat_buffer buffer_;
    boost::beast::http::request<boost::beast::http::string_body> req_;
    boost::beast::http::response<boost::beast::http::string_body> res_;
};