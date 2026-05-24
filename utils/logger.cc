#include"logger.h"
#include <iostream>
#include "Timestamp.h"

Logger& Logger::instance(){
    static Logger logger;
    return logger;
}
//设置日志等级//
void Logger::set_level(int level){
    log_level=level;
} 

//输出日志//
void Logger::log(std::string msg){
    switch (log_level)
    {   
    case DEBUG:
        std::cout<<"[DEBUG]: "<<msg<<std::endl;
        break;
    case INFO:
        std::cout<<"[INFO]: "<<msg<<std::endl;
        break;
    case FATAL:
        std::cout<<"[FATAL]: "<<msg<<std::endl;
        break;
    case ERROR:
        std::cout<<"[ERROR]: "<<msg<<std::endl; 
        break;
    case WARN:
        std::cout<<"[WARN]: "<<msg<<std::endl; 
        break;
    default:
        std::cout<<"[UNKNOWN LEVEL]: "<<msg<<std::endl;
        break;
    }
    std::cout<<Timestamp::now().toString()<<":"<<msg<<std::endl;

}