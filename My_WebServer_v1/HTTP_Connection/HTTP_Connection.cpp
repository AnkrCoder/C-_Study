#include "HTTP_Connection.h"
#include "../Logger/Logger.h"
#include <sstream>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <set>

HTTPConnection::HTTPConnection(TcpConnection &conn, std::string root_dir)
    : conn_(conn), root_dir_(std::move(root_dir)) {}

void HTTPConnection::handle_input(std::string &input_buffer)
{
    Logger::get_instance().log(Logger::INFO,
                               "Worker " + std::to_string(getpid()) +
                                   " handling fd=" + std::to_string(conn_.fd()));

    size_t header_end = input_buffer.find("\r\n\r\n");
    if (header_end == std::string::npos) // 检查头部信息是否完整
    {
        Logger::get_instance().log(Logger::WARNING,
                                   "Incomplete request headers from fd=" + std::to_string(conn_.fd()));
        return;
    }

    if (!parse_request(input_buffer.substr(0, header_end + 4)))
    {
        Logger::get_instance().log(Logger::ERROR,
                                   "Parse failed: " + input_buffer.substr(0, std::min(100ul, input_buffer.size())));
        send_response(HTTP_BAD_REQUEST, "<h1>400 Bad Request</h1>");
        // conn_.handle_close();    //潜在错误根源（核心转储）
        return;
    }

    input_buffer.erase(0, header_end + 4);
    prepare_response();
}

bool HTTPConnection::parse_request(const std::string &headers)
{
    std::istringstream iss(headers);
    std::string line;

    if (!std::getline(iss, line) || line.back() != '\r')
        return false;
    line.pop_back();

    std::istringstream req_line(line);
    if (!(req_line >> method_ >> uri_ >> version_))
        return false;

    // std::cout << "method: " << method_ << std::endl;
    // std::cout << "uri: " << uri_ << std::endl;

    std::transform(method_.begin(), method_.end(), method_.begin(), ::toupper);

    while (std::getline(iss, line) && line != "\r")
    {
        line.pop_back();
        size_t colon = line.find(':');
        if (colon == std::string::npos)
            continue;

        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        value.erase(0, value.find_first_not_of(' '));
        headers_[key] = value;
    }

    keep_alive_ = (version_ == "HTTP/1.1");
    if (headers_.count("Connection"))
    {
        keep_alive_ = (headers_["Connection"] == "keep-alive");
    }

    conn_.set_keep_alive(keep_alive_);
    return true;
}

void HTTPConnection::prepare_response()
{
    Logger::get_instance().log(Logger::INFO,
                               method_ + " " + uri_ + " (fd=" + std::to_string(conn_.fd()) + ")");

    if (uri_.find("..") != std::string::npos) // 防止路径遍历攻击
    {
        Logger::get_instance().log(Logger::WARNING, "Forbidden path: " + uri_);
        send_response(HTTP_FORBIDDEN, "<h1>403 Forbidden</h1>");
        return;
    }

    static const std::set<std::string> allowed_methods = {"GET", "HEAD", "POST"};
    if (allowed_methods.find(method_) == allowed_methods.end())
    {
        send_response(HTTP_METHOD_NOT_ALLOWED, "<h1>405 Method Not Allowed</h1>");
        return;
    }

    // 方法路由
    if (method_ == "GET")
    {
        handle_get();
    }
    else if (method_ == "HEAD")
    {
        handle_head();
    }
    else if (method_ == "POST")
    {
        handle_post();
    }
    else
    {
        send_response(HTTP_NOT_IMPLEMENTED, "<h1>501 Not Implemented</h1>");
    }
}

// GET方法实现
void HTTPConnection::handle_get()
{
    std::filesystem::path uri_path = (uri_ == "/") ? "/index.html" : uri_;
    std::filesystem::path full_path = std::filesystem::weakly_canonical(root_dir_ / uri_path.relative_path());

    std::string content;
    bool file_ok = read_file_content(full_path, content);

    if (!file_ok)
    {
        send_response(HTTP_NOT_FOUND, "<h1>404 Not Found</h1>");
        return;
    }

    std::string headers = "HTTP/1.1 200 OK\r\n";
    headers += "Content-Type: " + get_mime_type(full_path) + "\r\n";
    headers += "Content-Length: " + std::to_string(content.size()) + "\r\n";
    headers += "Connection: " + std::string(keep_alive_ ? "keep-alive" : "close") + "\r\n\r\n";

    // std::cout << "handle_get keep_alive_:" << keep_alive_ << std::endl;

    conn_.send(headers + content);
}

// HEAD方法实现
void HTTPConnection::handle_head()
{
    // std::cout << "handle_head started:" << std::endl;

    std::filesystem::path uri_path = (uri_ == "/") ? "/index.html" : uri_;
    std::filesystem::path full_path = std::filesystem::weakly_canonical(root_dir_ / uri_path.relative_path());

    std::string content;
    bool file_ok = read_file_content(full_path, content);

    std::string headers = file_ok ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 404 Not Found\r\n";

    headers += "Content-Type: " + get_mime_type(full_path) + "\r\n";
    headers += "Content-Length: " + std::to_string(content.size()) + "\r\n";
    headers += "Connection: " + std::string(keep_alive_ ? "keep-alive" : "close") + "\r\n\r\n";

    // std::cout << "handle_head keep_alive_:" << keep_alive_ << std::endl;

    conn_.send(headers); // 仅发送头部
}

// POST方法实现
void HTTPConnection::handle_post()
{
    std::string response_body = "<h1>POST Received</h1>";
    std::string headers = "HTTP/1.1 200 OK\r\n";
    headers += "Content-Type: text/html\r\n";
    headers += "Content-Length: " + std::to_string(response_body.size()) + "\r\n";
    headers += "Connection: " + std::string(keep_alive_ ? "keep-alive" : "close") + "\r\n\r\n";

    // std::cout << "handle_post keep_alive_:" << keep_alive_ << std::endl;
    conn_.send(headers + response_body);
}

void HTTPConnection::send_response(int status, const std::string &content)
{
    Logger::get_instance().log(Logger::INFO, "Response " + std::to_string(status) + " for " + uri_);

    std::map<int, std::string> status_text = {
        {HTTP_OK, "OK"},
        {HTTP_BAD_REQUEST, "Bad Request"},
        {HTTP_FORBIDDEN, "Forbidden"},
        {HTTP_NOT_FOUND, "Not Found"},
        {HTTP_METHOD_NOT_ALLOWED, "Method Not Allowed"},
        {HTTP_INTERNAL_ERROR, "Internal Server Error"},
        {HTTP_NOT_IMPLEMENTED, "Not Implemented"}};

    std::string headers = "HTTP/1.1 " +
                          std::to_string(status) + " " +
                          status_text[status] + "\r\n";

    headers += "Content-Type: text/html\r\n";
    headers += "Content-Length: " + std::to_string(content.size()) + "\r\n";
    headers += "Connection: close\r\n\r\n";

    keep_alive_ = false;

    conn_.set_keep_alive(keep_alive_);
    conn_.send(headers + content);
}

bool HTTPConnection::read_file_content(const std::string &path, std::string &content) const
{
    if (!std::filesystem::exists(path))
    {
        return false;
    }

    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    if (!file)
    {
        return false;
    }

    // 使用 filesystem 获取文件大小
    auto file_size = std::filesystem::file_size(path);
    content.resize(file_size);

    // 读取文件内容
    file.read(content.data(), file_size);

    return true;
}

std::string HTTPConnection::get_mime_type(const std::string &path) const
{
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return "text/plain";

    std::string ext = path.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    static const std::map<std::string, std::string> mime_types = {
        {"html", "text/html"},
        {"css", "text/css"},
        {"js", "application/javascript"},
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"}};

    auto it = mime_types.find(ext);
    return (it != mime_types.end()) ? it->second : "text/plain";
}
