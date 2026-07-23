#pragma once
#include "noncopy.h"
#include <string>
#include <cstdio>

#define LOG_INFO(LogmsgFormat , ...) \
    do { \
        Logger &logger = Logger::instance(); \
        logger.set_level(INFO); \
        char buffer[1024]; \
        std::snprintf(buffer,1024 ,LogmsgFormat,##__VA_ARGS__); \
        logger.log(buffer); \
    } while (0)
#define LOG_WARN(LogmsgFormat , ...) \
    do { \
        Logger &logger = Logger::instance(); \
        logger.set_level(WARN); \
        char buffer[1024]; \
        std::snprintf(buffer,1024 ,LogmsgFormat,##__VA_ARGS__); \
        logger.log(buffer); \
    } while (0)
    
#define LOG_ERROR(LogmsgFormat , ...) \
    do { \
        Logger &logger = Logger::instance(); \
        logger.set_level(ERROR); \
        char buffer[1024]; \
        std::snprintf(buffer,1024 ,LogmsgFormat,##__VA_ARGS__); \
        logger.log(buffer); \
    } while (0)

#define LOG_FATAL(LogmsgFormat , ...) \
    do { \
        Logger &logger = Logger::instance(); \
        logger.set_level(FATAL); \
        char buffer[1024]; \
        std::snprintf(buffer,1024 ,LogmsgFormat,##__VA_ARGS__); \
        logger.log(buffer); \
        exit(-1); \
    } while (0)
    
#ifdef MUDEBUG
#define LOG_DEBUG(LogmsgFormat , ...) \
    do { \
        Logger &logger = Logger::instance(); \
        logger.set_level(DEBUG); \
        char buffer[1024]; \
        std::snprintf(buffer,1024 ,LogmsgFormat,##__VA_ARGS__); \
        logger.log(buffer); \
    } while (0)
#else
    #define LOG_DEBUG(LogmsgFormat , ...)
#endif
// 日志类型
enum LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};

// 输出一个日志类
class Logger : NonCopyable {
public:
    static Logger& instance();
    void set_level(int level);
    void log(std::string msg);
private:
    int log_level;
    Logger() {};
};