#include <sys/wait.h>
#include <unistd.h>
#include <csignal>
#include <vector>
#include <iostream>
#include <atomic>
#include <cstring>

std::atomic<bool> master_keep_running{true}; // 主进程控制标志

// Worker进程信号处理函数
void worker_signal_handler(int sig)
{
    std::cout << "Worker " << getpid() << " received signal " << sig << "\n";
    // 设置退出标志
    master_keep_running = false;
}

// Worker进程执行逻辑
void worker_process(int worker_id)
{
    // 设置信号处理
    struct sigaction sa;
    sa.sa_handler = worker_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, nullptr); // 捕获SIGTERM

    // 忽略SIGINT
    struct sigaction sa_ignore;
    sa_ignore.sa_handler = SIG_IGN;
    sigemptyset(&sa_ignore.sa_mask);
    sa_ignore.sa_flags = 0;
    sigaction(SIGINT, &sa_ignore, nullptr);

    std::cout << "Worker " << worker_id
              << " (PID:" << getpid() << ") started\n";

    // 正确的工作循环
    while (master_keep_running)
    {
        // 实际工作代码
        sleep(1);
    }

    std::cout << "Worker " << worker_id << " exiting\n";
    exit(0); // 正常退出
}

// Master进程管理类
class ProcessMaster
{
public:
    ProcessMaster(int workers) : worker_count(workers)
    {
        // 注册信号处理
        struct sigaction sa;
        sa.sa_handler = signal_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;

        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);
    }

    void run()
    {
        init_system();
        create_workers();
        monitor_workers();
    }

private:
    void init_system()
    {
        std::cout << "Master (PID:" << getpid()
                  << ") initializing...\n";
        // 初始化日志、Reactor等
    }

    void create_workers()
    {
        for (int i = 0; i < worker_count; ++i)
        {
            pid_t pid = fork();
            if (pid == 0)
            { // 子进程
                worker_process(i);
            }
            else if (pid > 0)
            { // 父进程
                workers.push_back(pid);
                std::cout << "Created worker " << i
                          << " (PID:" << pid << ")\n";
            }
            else
            {
                std::cerr << "Fork failed!\n";
            }
        }
    }

    void monitor_workers()
    {
        // 修改后的信号处理循环
        sigset_t mask, orig_mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGINT);
        sigaddset(&mask, SIGTERM);
        sigprocmask(SIG_BLOCK, &mask, &orig_mask);

        while (master_keep_running)
        {
            // 使用sigsuspend等待信号
            sigsuspend(&orig_mask);
        }

        // 终止所有Worker
        for (auto pid : workers)
        {
            kill(pid, SIGTERM);
        }

        // 等待所有Worker退出
        for (auto pid : workers)
        {
            int status;
            waitpid(pid, &status, 0);
            std::cout << "Worker " << pid << " exited\n";
        }

        std::cout << "Master process exit\n";
    }

    static void signal_handler(int)
    {
        std::cout << "\nReceived shutdown signal\n";
        master_keep_running = false;
    }

    const int worker_count;
    std::vector<pid_t> workers;
};

int main()
{
    ProcessMaster master(3); // 创建3个Worker
    master.run();
    return 0;
}