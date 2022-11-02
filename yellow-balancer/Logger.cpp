#include "logger.h"

using namespace std;

Logger* Logger::logger_ = nullptr;
LoggerDestroyer Logger::destroyer_;
static mutex get_instance_;
static mutex print_;

LoggerDestroyer::~LoggerDestroyer() {
    delete logger_;
}

void LoggerDestroyer::initialize(Logger* logger) {
    logger_ = logger;
}

Logger* Logger::getInstance() {
    lock_guard<mutex> guard(get_instance_);
    if (!logger_) {
        logger_ = new Logger();
        logger_->time_buffer_ = string(24, '\0');
        logger_->minimum_type_ = Type::Error;
        logger_->out_console_ = false;
        destroyer_.initialize(logger_);
    }
    return logger_;
}

Logger::~Logger() {
    Print(L"LOGGER DESTROYER!!!", Logger::Type::Trace);
    if (fs_.is_open()) {
        fs_.close();
    }
}

int Logger::CurHour() {
    chrono::system_clock::time_point now = chrono::system_clock::now();
    time_t time = chrono::system_clock::to_time_t(now);
    struct tm tm;
    localtime_s(&tm, &time);
    return tm.tm_hour;
}

wstring Logger::LogFileName() {
    chrono::system_clock::time_point now = chrono::system_clock::now();
    time_t time = chrono::system_clock::to_time_t(now);
    struct tm tm;
    localtime_s(&tm, &time);
    cur_hour_ = tm.tm_hour;
    vector<char> buffer(20);
    strftime(&buffer[0], buffer.size(), "%y%m%d%H", &tm);
    return Utf8ToWideChar(string(&buffer[0]).append(".log"));
}

void DeleteLogHistory(filesystem::path dir, int log_storage_duration) {
    chrono::system_clock::time_point now = chrono::system_clock::now() - chrono::hours(log_storage_duration);
    time_t time = chrono::system_clock::to_time_t(now);
    struct tm tm;
    localtime_s(&tm, &time);
    vector<char> buffer(20);
    strftime(&buffer[0], buffer.size(), "%y%m%d%H", &tm);
    wstring max_file_name = Utf8ToWideChar(string(&buffer[0]).append(".log"));

    if (filesystem::exists(dir)) {
        for (const filesystem::directory_entry& it : filesystem::recursive_directory_iterator(dir)) {
            auto ss = it.path().filename().wstring();
            if (it.is_regular_file() && it.path().extension().string() == ".log" && it.path().filename().wstring() < max_file_name) {
                error_code ec;
                filesystem::remove(it.path(), ec);
                if (ec) {
                    //TODO сдесь запись логгер писать будет в другом потоке, нужен mutex в процедуре записи
                }
            }
        }
    }
}

void Logger::Open(filesystem::path dir) {
    dir_ = dir;
    log_storage_duration_ = 24;
    if (!fs_.is_open()) {
        wstring file_name = LogFileName();
        dir.append(L"logs");
        filesystem::directory_entry dir_logs(dir);
        if (!dir_logs.exists()) {
            filesystem::create_directory(dir_logs);
        }
        dir.append(file_name);
        fs_.open(dir, ios::out | ios::app | ios::binary);
        thread thread(DeleteLogHistory, dir.parent_path(), log_storage_duration_);
        thread.detach();
    }
}

void Logger::SetLogStorageDuration(int log_storage_duration) {
    log_storage_duration_ = log_storage_duration;
}

const string& Logger::CurTime() {
    chrono::system_clock::time_point now = chrono::system_clock::now();
    time_t time = chrono::system_clock::to_time_t(now);
    const chrono::duration<double> tse = now.time_since_epoch();
    chrono::seconds::rep milliseconds = chrono::duration_cast<chrono::milliseconds>(tse).count() % 1000;
    ss_.str("");
    ss_.clear();
    ss_ << setw(3) << setfill('0') << milliseconds;

    struct tm new_time;
    localtime_s(&new_time, &time);
    strftime(&time_buffer_[0], time_buffer_.size(), "%Y-%m-%d %H:%M:%S", &new_time);
    time_buffer_.replace(19, 1, ".");
    time_buffer_.replace(20, 3, ss_.str());
    time_buffer_.replace(23, 1, ";");

    return time_buffer_;
}

void Logger::Print(const string& msg, Logger::Type type, bool anyway) {
    Print(Utf8ToWideChar(msg), type, anyway);
}

void Logger::Print(const wstring& msg, Logger::Type type, bool anyway) {
    lock_guard<mutex> guard(print_);
    if (cur_hour_ != CurHour()) {
        if (fs_.is_open()) {
            fs_.close();
        }
        Open(dir_);
    }

    if (!anyway && type < minimum_type_) {
        return;
    }

    const string& cur_time = CurTime();
    fs_ << cur_time;
    if (out_console_) wcout << Utf8ToWideChar(cur_time);

    switch (type)
    {
    case Type::Trace:
        fs_ << u8"TRACE;";
        if (out_console_) wcout << L"TRACE;";
        break;
    case Type::Info:
        fs_ << u8"INFO;";
        if (out_console_) wcout << L"INFO;";
        break;
    case Type::Error:
        fs_ << u8"ERROR;";
        if (out_console_) wcout << L"ERROR;";
        break;
    default:
        break;
    }

    fs_ << WideCharToUtf8(msg);
    if (out_console_) wcout << msg;

    fs_ << '\n';
    if (out_console_) wcout << L"\n";

    fs_.flush();
}

void Logger::Print(const string& msg, bool anyway) {
    Print(msg, Type::Info, anyway);
}

void Logger::Print(const wstring& msg, bool anyway) {
    Print(msg, Type::Info, anyway);
}

void Logger::SetOutConsole(bool out_console) {
    out_console_ = out_console;
}