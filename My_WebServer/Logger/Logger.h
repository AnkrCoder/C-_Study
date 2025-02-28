#pragma once

#include <string>
#include <fstream>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <atomic>

class Logger
{
public:
    enum LogLevel
    {
        INFO,
        WARNING,
        ERROR
    };

    static Logger &get_instance(); // 单例模式获取实例

    void init(const std::string &log_dir = "logging",
              int max_lines = 8000);

    void log(LogLevel level, const std::string &message);   // 写日志函数

private:
    Logger() = default;
    ~Logger();

    void check_file();  // 检查是否需要切换文件
    void rotate_file(); // 切换文件

    std::string get_time_string(const char *format);    // 获取时间字符串

    std::ofstream log_file_;    // 日志文件
    std::string log_dir_;   // 日志文件目录

    int max_lines_;

    std::atomic<int> current_lines_{0};
    std::string current_date_;
    int file_index_{0};
    std::mutex mutex_;
};
