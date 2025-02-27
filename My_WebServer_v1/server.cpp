#include "Master_Worker/master_worker.h"
#include <iostream>

// 创建监听套接字，用于worker进程接收客户端连接
int create_socket(int port)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1)
    {
        throw std::system_error(errno, std::generic_category(), "socket");
    }

    // 设置端口复用，防止程序异常退出后，端口占用时间过长
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_fd, (sockaddr *)&addr, sizeof(addr)) == -1)
    {
        close(listen_fd);
        throw std::system_error(errno, std::generic_category(), "bind");
    }

    if (listen(listen_fd, SOMAXCONN) == -1)
    {
        close(listen_fd);
        throw std::system_error(errno, std::generic_category(), "listen");
    }

    return listen_fd;
}

int main()
{
    int listen_fd = create_socket(8080);

    try
    {
        Logger::get_instance().init("logging", 1000);   // 初始化日志系统

        ProcessMaster master(3); // 初始化Master进程，准备创建3个Worker进程
        Logger::get_instance().log(Logger::INFO, "Master: " + std::to_string(getpid()) + " started");
        master.run(listen_fd);   // 启动Master进程
    }
    catch (const std::exception &e)
    {
        Logger::get_instance().log(Logger::ERROR, e.what());
        // std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
