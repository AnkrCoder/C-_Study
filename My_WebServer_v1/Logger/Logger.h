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

    static Logger &get_instance();

    void init(const std::string &log_dir = "logging",
              int max_lines = 1000);
    void log(LogLevel level, const std::string &message);

private:
    Logger() = default;
    ~Logger();

    void check_file();//检查文件

    void rotate_file();
    std::string get_time_string(const char *format);    //获取时间字符串

    std::ofstream log_file_;    //日志文件
    std::string log_dir_;   //日志目录
    int max_lines_; //最大行数
    std::atomic<int> current_lines_{0}; //当前行数
    std::string current_date_;  //当前日期
    int file_index_{0}; //文件索引
    std::mutex mutex_;
};
