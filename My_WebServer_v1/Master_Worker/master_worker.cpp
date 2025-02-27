#include "master_worker.h"

#include <sys/wait.h>
#include <unistd.h>
#include <csignal>
#include <vector>
#include <iostream>
#include <atomic>
#include <cstring>

bool ProcessMaster::master_keep_running = true;

// 全局变量，用于存储 reactor 指针
static EpollReactor *g_reactor = nullptr;

// Worker进程信号处理函数
void worker_signal_handler(int sig)
{
    std::cout << "Worker " << getpid() << " received signal " << sig << "\n";

    if (g_reactor)
    {
        g_reactor->stop();
    }
}

// Worker进程执行逻辑
void worker_process(int worker_id, int listen_fd)
{
    // 捕获SIGTERM信号
    struct sigaction sa;
    sa.sa_handler = worker_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, nullptr);

    // 忽略SIGINT信号
    struct sigaction sa_ignore;
    sa_ignore.sa_handler = SIG_IGN;
    sigemptyset(&sa_ignore.sa_mask);
    sa_ignore.sa_flags = 0;
    sigaction(SIGINT, &sa_ignore, nullptr);

    // 工作循环
    EpollReactor reactor;
    g_reactor = &reactor;
    TcpAcceptor acceptor(reactor, listen_fd);

    acceptor.set_new_connection_callback([&reactor](int fd)
                                         {
            auto conn = TcpConnection::create(fd, reactor);
            auto http_conn = std::make_shared<HTTPConnection>(*conn, "./root");
            
            conn->set_read_callback([http_conn](std::string& buf) {
                http_conn->handle_input(buf);
            });
            
            conn->start(); });

    Logger::get_instance().log(Logger::INFO, "Worker " + std::to_string(getpid()) +
                                                 " started with listen_fd=" + std::to_string(listen_fd));

    reactor.run(); // 进入事件循环

    std::cout << "Worker " << worker_id << " exiting\n";
    g_reactor = nullptr;

    exit(0); // 正常退出
}

ProcessMaster::ProcessMaster(int workers) : worker_count(workers)
{
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

void ProcessMaster::run(int listen_fd)
{
    create_workers(listen_fd);
    monitor_workers();
}

// 创建Worker进程
void ProcessMaster::create_workers(int listen_fd)
{
    for (int i = 0; i < worker_count; ++i)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            Logger::get_instance().log(Logger::INFO, "Worker " + std::to_string(i) + " started");
            worker_process(i, listen_fd);
            exit(EXIT_SUCCESS);
        }
        else if (pid > 0)
        {
            workers.push_back(pid);
        }
        else
        {
            throw std::runtime_error("fork failed");
        }
    }
}

// 监控Worker进程
void ProcessMaster::monitor_workers()
{
    sigset_t mask, orig_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, &orig_mask); // 屏蔽SIGINT、SIGTERM信号

    while (master_keep_running)
    {
        sigsuspend(&orig_mask); // 挂起进程，等待信号
    }

    for (auto pid : workers)
    {
        kill(pid, SIGTERM); // 向Worker进程发送SIGTERM信号
    }

    for (auto pid : workers)
    {
        int status;
        waitpid(pid, &status, 0); // 等待Worker进程退出
        std::cout << "Worker " << pid << " exited\n";
    }

    std::cout << "Master process exit\n";
}

void ProcessMaster::signal_handler(int sig)
{
    std::cout << "\nReceived shutdown signal" << std::endl;
    master_keep_running = false;
}
