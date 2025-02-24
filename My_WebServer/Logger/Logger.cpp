// Logger.cpp
#include "Logger.h"
#include <iomanip>
#include <stdexcept>
#include <iostream>

Logger &Logger::get_instance()
{
    static Logger instance;
    return instance;
}

Logger::~Logger()
{
    if (log_file_.is_open())
    {
        log_file_.close();
    }
}

void Logger::init(const std::string &log_dir, int max_lines)
{
    std::lock_guard<std::mutex> lock(mutex_);
    current_lines_.store(0);
    log_dir_ = log_dir;
    max_lines_ = max_lines;

    // 创建日志目录
    if (!std::filesystem::exists(log_dir_))
    {
        std::filesystem::create_directories(log_dir_);
    }

    try
    {
        check_file();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Logger init failed: " << e.what() << std::endl;
        throw;
    }
}

void Logger::log(LogLevel level, const std::string &message)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!log_file_.is_open())
        return;

    // 获取当前时间
    time_t now = time(nullptr);
    tm *local_time = localtime(&now);

    // 格式化日志级别字符串
    const char *level_str = "";
    switch (level)
    {
    case INFO:
        level_str = "INFO";
        break;
    case WARNING:
        level_str = "WARNING";
        break;
    case ERROR:
        level_str = "ERROR";
        break;
    }

    // 构建日志条目
    log_file_ << "[" << get_time_string("%Y-%m-%d %H:%M:%S") << "] "
              << "[" << level_str << "] "
              << message << "\n";

    log_file_.flush();
    if (log_file_.fail())
    {
        std::cerr << "Write to log file failed!" << std::endl;
        log_file_.clear();
    }

    current_lines_++;

    // 检查是否需要切换文件
    if (current_lines_ >= max_lines_)
    {
        rotate_file();
    }
}

void Logger::check_file()
{
    std::string today = get_time_string("%Y-%m-%d");

    // 如果日期或文件索引变化需要切换文件
    if (today != current_date_)
    {
        current_date_ = today;
        file_index_ = 0;
        rotate_file();
    }
    else if (!log_file_.is_open())
    {
        rotate_file();
    }
}

void Logger::rotate_file()
{
    if (log_file_.is_open())
    {
        log_file_.close();
    }

    // 生成新文件名
    std::string filename;
    do
    {
        filename = log_dir_ + "/" + current_date_;
        if (file_index_ > 0)
        {
            filename += "_" + std::to_string(file_index_);
        }
        filename += ".log";
        file_index_++;
    } while (std::filesystem::exists(filename));

    log_file_.open(filename, std::ios::app);

    // 检查文件是否成功打开
    if (!log_file_.is_open())
    {
        throw std::runtime_error("Failed to open log file: " + filename);
    }

    log_file_ << "[" << get_time_string("%Y-%m-%d %H:%M:%S") << "] [SYSTEM] Log file created\n";
    log_file_.flush();

    current_lines_ = 0;
}

std::string Logger::get_time_string(const char *format)
{
    time_t now = time(nullptr);
    tm *local_time = localtime(&now);
    char buffer[80];
    strftime(buffer, sizeof(buffer), format, local_time);
    return buffer;
}
