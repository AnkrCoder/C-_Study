#include "http_connection.h"
#include <boost/beast/http.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <fstream>
#include <iostream>

HttpConnection::HttpConnection(boost::asio::ip::tcp::socket socket)
    : socket_(std::move(socket)) {}

void HttpConnection::start() {
    do_read();
}

void HttpConnection::do_read() {
    boost::beast::http::read(socket_, buffer_, req_);
    handle_request(std::move(req_));
}

void HttpConnection::handle_request(boost::beast::http::request<boost::beast::http::string_body>&& req) {
    if (req.method() == boost::beast::http::verb::get) {
        std::ifstream file("./root/index.html");
        if (file) {
            std::string body((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            res_.result(boost::beast::http::status::ok);
            res_.version(req.version());
            res_.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
            res_.set(boost::beast::http::field::content_type, "text/html");
            res_.body() = body;
            res_.prepare_payload();
        } else {
            res_.result(boost::beast::http::status::not_found);
            res_.version(req.version());
            res_.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
            res_.set(boost::beast::http::field::content_type, "text/html");
            res_.body() = "File not found";
            res_.prepare_payload();
        }
    } else {
        res_.result(boost::beast::http::status::bad_request);
        res_.version(req.version());
        res_.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
        res_.set(boost::beast::http::field::content_type, "text/html");
        res_.body() = "Unsupported HTTP method";
        res_.prepare_payload();
    }

    send_response(std::move(res_));
}

void HttpConnection::send_response(boost::beast::http::response<boost::beast::http::string_body>&& res) {
    boost::beast::http::write(socket_, res);
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send);
}
