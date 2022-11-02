#pragma once

#include <iostream>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <windows.h>
#include <thread>
#include <mutex>
#include "encoding_string.h"

class Logger;

class LoggerDestroyer
{
private:
    Logger* logger_;
public:
    ~LoggerDestroyer();
    void initialize(Logger* logger);
};

class Logger {
public:
    enum Type { All, Trace, Info, Error };
private:
    std::string time_buffer_;
    std::ostringstream ss_;
    bool out_console_;
    static Logger* logger_;
    static LoggerDestroyer destroyer_;
    Logger::Type minimum_type_;
    const std::string& CurTime();
    std::ofstream fs_;
    int cur_hour_;
    std::wstring LogFileName();
    int CurHour();
    std::filesystem::path dir_;
    int log_storage_duration_;
protected:
    Logger() {}
    Logger(const Logger&);
    Logger& operator=(Logger&);
    ~Logger();
    friend class LoggerDestroyer;
public:
    static Logger* getInstance();
    void Open(std::filesystem::path dir);
    void SetLogStorageDuration(int log_storage_duration);

    void Print(const std::string& msg, bool anyway = false);
    void Print(const std::wstring& msg, bool anyway = false);
    void Print(const std::string& msg, Logger::Type type, bool anyway = false);
    void Print(const std::wstring& msg, Logger::Type type, bool anyway = false);

    void SetOutConsole(bool out_console);
    void SetLogType(Type type) { minimum_type_ = type; }
    Type LogType() { return minimum_type_; }
};
