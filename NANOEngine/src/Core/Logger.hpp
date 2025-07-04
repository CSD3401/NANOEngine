#ifndef LOGGER_HPP
#define LOGGER_HPP

#ifdef APIENTRY
#undef APIENTRY
#endif

#include <iostream>
#include <sstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <string>
#include <windows.h>

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

class Logger {
public:
    static Logger& GetInstance() {
        static Logger instance;
        return instance;
    }

    void SetMinLogLevel(LogLevel level) { minLogLevel = level; }

    void Log(LogLevel level, const std::string& message,
        const std::string& file = "", int line = -1) {
        if (level < minLogLevel) return;

        std::lock_guard<std::mutex> lock(logMutex);
        std::string formatted = FormatMessage(level, message, file, line);

        SetConsoleColor(level);
        std::cout << formatted;
        ResetConsoleColor();
    }

private:
    Logger() {

        hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        GetConsoleScreenBufferInfo(hConsole, &originalConsoleInfo);
        originalAttributes = originalConsoleInfo.wAttributes;
    }

    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string FormatMessage(LogLevel level, const std::string& message,
        const std::string& file, int line) {
        // Get timestamp
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm;

        localtime_s(&tm, &time);

        std::stringstream ss;
        ss << std::put_time(&tm, "[%Y-%m-%d %H:%M:%S]");

        // Log level string
        const char* levelStr = "";
        switch (level) {
        case LogLevel::Debug:    levelStr = "[DEBUG]   "; break;
        case LogLevel::Info:     levelStr = "[INFO]    "; break;
        case LogLevel::Warning:  levelStr = "[WARNING] "; break;
        case LogLevel::Error:    levelStr = "[ERROR]   "; break;
        case LogLevel::Critical: levelStr = "[CRITICAL]"; break;
        }

        // File and line
        std::string fileLine;
        if (!file.empty() && line != -1) {
            size_t lastSlash = file.find_last_of("/\\");
            std::string filename = (lastSlash == std::string::npos)
                ? file
                : file.substr(lastSlash + 1);
            fileLine = "[" + filename + ":" + std::to_string(line) + "]";
        }

        // Combine all parts
        ss << " " << levelStr << " " << fileLine << " " << message << "\n";
        return ss.str();
    }

    void SetConsoleColor(LogLevel level) {
        WORD color;
        switch (level) {
        case LogLevel::Debug:    // Gray
            color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
            break;
        case LogLevel::Info:     // White
            color = originalAttributes;
            break;
        case LogLevel::Warning:  // Yellow
            color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            break;
        case LogLevel::Error:    // Bright Red
            color = FOREGROUND_RED | FOREGROUND_INTENSITY;
            break;
        case LogLevel::Critical: // Red background, white text
            color = BACKGROUND_RED | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            break;
        default:
            color = originalAttributes;
        }
        SetConsoleTextAttribute(hConsole, color);
    }

    void ResetConsoleColor() {
        SetConsoleTextAttribute(hConsole, originalAttributes);
    }

    LogLevel minLogLevel = LogLevel::Debug;
    std::mutex logMutex;

    HANDLE hConsole;
    CONSOLE_SCREEN_BUFFER_INFO originalConsoleInfo;
    WORD originalAttributes;
};

// Helper macros for easy logging
#define LOG_DEBUG(...)   do { std::ostringstream oss; oss << __VA_ARGS__; \
    Logger::GetInstance().Log(LogLevel::Debug, oss.str(), __FILE__, __LINE__); } while(0)
#define LOG_INFO(...)    do { std::ostringstream oss; oss << __VA_ARGS__; \
    Logger::GetInstance().Log(LogLevel::Info, oss.str(), __FILE__, __LINE__); } while(0)
#define LOG_WARNING(...) do { std::ostringstream oss; oss << __VA_ARGS__; \
    Logger::GetInstance().Log(LogLevel::Warning, oss.str(), __FILE__, __LINE__); } while(0)
#define LOG_ERROR(...)   do { std::ostringstream oss; oss << __VA_ARGS__; \
    Logger::GetInstance().Log(LogLevel::Error, oss.str(), __FILE__, __LINE__); } while(0)
#define LOG_CRITICAL(...) do { std::ostringstream oss; oss << __VA_ARGS__; \
    Logger::GetInstance().Log(LogLevel::Critical, oss.str(), __FILE__, __LINE__); } while(0)

#endif
