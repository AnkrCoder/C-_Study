// Logger.h
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

    void check_file();
    void rotate_file();
    std::string get_time_string(const char *format);

    std::ofstream log_file_;
    std::string log_dir_;
    int max_lines_;
    std::atomic<int> current_lines_{0};
    std::string current_date_;
    int file_index_{0};
    std::mutex mutex_;
};
