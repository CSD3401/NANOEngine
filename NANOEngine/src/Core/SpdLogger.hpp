/*!
\file       SpdLogger.hpp
\author     Anson Teng
\date       9/9/2025
\brief      This file contains declarations for SpdLogger wrapper around spdlog library.
            Provides unified logging interface with ImGui panel integration and crash-only file logging.
Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/

#ifndef SPDLOG_LOGGER_HPP
#define SPDLOG_LOGGER_HPP

#ifdef APIENTRY
#undef APIENTRY
#endif

#define FMT_UNICODE 0  // Disable Unicode to avoid UTF-8 requirement

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/base_sink.h>
#include <memory>
#include <sstream>
#include <vector>
#include <deque>
#include <mutex>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <csignal>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")
#endif

enum class SpdLogLevel {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Critical = 4
};

struct SpdLogEntry {
    SpdLogLevel level;
    std::string message;
    std::string file;
    int line;
    std::chrono::system_clock::time_point timestamp;
    std::string formattedMessage;
};

// Custom sink to capture logs for the panel
template<typename Mutex>
class panel_sink : public spdlog::sinks::base_sink<Mutex> {
public:
    void get_logs(std::vector<SpdLogEntry>& out_logs) {
        std::lock_guard<Mutex> lock(this->mutex_);
        out_logs.assign(logs_.begin(), logs_.end());
    }
    
    void clear_logs() {
        std::lock_guard<Mutex> lock(this->mutex_);
        logs_.clear();
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        // don't use the base class mutex here to avoid deadlock
        std::lock_guard<std::mutex> lock(logs_mutex_);
        
        SpdLogEntry entry;
        entry.level = static_cast<SpdLogLevel>(static_cast<int>(msg.level));
        entry.message = std::string(msg.payload.data(), msg.payload.size());
        entry.file = msg.source.filename ? std::string(msg.source.filename) : "";
        entry.line = msg.source.line;
        entry.timestamp = std::chrono::system_clock::time_point(
            std::chrono::duration_cast<std::chrono::system_clock::duration>(msg.time.time_since_epoch()));
        
        // format the message using spdlog formatter
        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);
        entry.formattedMessage = std::string(formatted.data(), formatted.size());
        
        logs_.push_back(entry);
        
        // Limit the number of stored entries
        if (logs_.size() > max_logs_) {
            logs_.pop_front();
        }
    }

    void flush_() override {}

private:
    std::deque<SpdLogEntry> logs_;
    std::mutex logs_mutex_;
    static const size_t max_logs_ = 1000;
};

using panel_sink_mt = panel_sink<std::mutex>;

class SpdLogger {
public:
    static SpdLogger& GetInstance() {
        static SpdLogger instance;
        return instance;
    }

    void SetMinLogLevel(SpdLogLevel level) {
        spdlog::set_level(static_cast<spdlog::level::level_enum>(static_cast<int>(level)));
    }

    void Log(SpdLogLevel level, const std::string& message,
        const std::string& file = "", int line = -1) {
        
        std::string filename;
        if (!file.empty()) {
            size_t lastSlash = file.find_last_of("/\\");
            filename = (lastSlash == std::string::npos) ? file : file.substr(lastSlash + 1);
        }

        switch (level) {
        case SpdLogLevel::Debug:
            if (line != -1) {
                spdlog::log(spdlog::source_loc{filename.c_str(), line, ""}, 
                           spdlog::level::debug, message);
            } else {
                spdlog::debug(message);
            }
            break;
        case SpdLogLevel::Info:
            if (line != -1) {
                spdlog::log(spdlog::source_loc{filename.c_str(), line, ""}, 
                           spdlog::level::info, message);
            } else {
                spdlog::info(message);
            }
            break;
        case SpdLogLevel::Warning:
            if (line != -1) {
                spdlog::log(spdlog::source_loc{filename.c_str(), line, ""}, 
                           spdlog::level::warn, message);
            } else {
                spdlog::warn(message);
            }
            break;
        case SpdLogLevel::Error:
            if (line != -1) {
                spdlog::log(spdlog::source_loc{filename.c_str(), line, ""}, 
                           spdlog::level::err, message);
            } else {
                spdlog::error(message);
            }
            break;
        case SpdLogLevel::Critical:
            if (line != -1) {
                spdlog::log(spdlog::source_loc{filename.c_str(), line, ""}, 
                           spdlog::level::critical, message);
            } else {
                spdlog::critical(message);
            }
            break;
        }
    }

    std::vector<SpdLogEntry> GetLogEntries() const {
        std::vector<SpdLogEntry> entries;
        if (panel_sink_) {
            panel_sink_->get_logs(entries);
        }
        return entries;
    }

    void ClearLogEntries() {
        if (panel_sink_) {
            panel_sink_->clear_logs();
        }
    }

    /*!
    \brief Enables crash-only logging mode with automatic crash detection
    \param crashLogPath Directory to save crash logs (created if doesn't exist)
    */
    void EnableCrashOnlyLogging(const std::string& crashLogPath = "crash_logs/") {
        m_crashLogPath = crashLogPath;
        m_crashOnlyMode = true;
        
        // Create crash logs directory
        try {
            std::filesystem::create_directories(crashLogPath);
        } catch (...) {
            // If directory creation fails, use current directory
            m_crashLogPath = "./";
        }
        
        // Set up automatic crash detection
        SetupAutomaticCrashDetection();
        
        spdlog::info("Crash-only logging enabled with automatic detection - logs will be saved to {} when crashes occur", crashLogPath);
    }

    /*!
    \brief Sets up automatic crash detection using signal handlers and Windows exceptions
    */
    void SetupAutomaticCrashDetection() {
        // Set up signal handlers for common crashes
        signal(SIGABRT, HandleCrashSignal);
        signal(SIGFPE, HandleCrashSignal);
        signal(SIGILL, HandleCrashSignal);
        signal(SIGINT, HandleCrashSignal);
        signal(SIGSEGV, HandleCrashSignal);
        signal(SIGTERM, HandleCrashSignal);

#ifdef _WIN32
        // Windows-specific unhandled exception handler
        SetUnhandledExceptionFilter(HandleWindowsException);
#endif

        spdlog::info("Automatic crash detection enabled");
    }

    /*!
    \brief Saves current logs to a crash file with timestamp and system information
    \param crashReason Description of what caused the crash
    */
    void SaveCrashLog(const std::string& crashReason = "Manual crash log requested") {
        if (!m_crashOnlyMode) {
            spdlog::warn("SaveCrashLog called but crash-only mode not enabled");
            return;
        }
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_s(&tm, &time_t);
        
        std::stringstream filename;
        filename << m_crashLogPath << "crash_" 
                 << std::put_time(&tm, "%Y%m%d_%H%M%S") 
                 << ".log";
        
        std::ofstream crashFile(filename.str());
        if (!crashFile.is_open()) {
            spdlog::error("Failed to create crash log file: {}", filename.str());
            return;
        }
        
        // Write crash header
        crashFile << "=== NANO ENGINE CRASH LOG ===" << std::endl;
        crashFile << "Timestamp: " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << std::endl;
        crashFile << "Crash Reason: " << crashReason << std::endl;
        crashFile << std::endl;

        // Write system information
        crashFile << "=== SYSTEM INFORMATION ===" << std::endl;
        crashFile << GatherSystemInfo() << std::endl;
        
        // Get recent logs from memory
        auto logs = GetLogEntries();
        crashFile << "=== RECENT LOGS (Last " << logs.size() << " entries) ===" << std::endl;
        
        for (const auto& entry : logs) {
            auto logTime = std::chrono::system_clock::to_time_t(entry.timestamp);
            std::tm logTm;
            localtime_s(&logTm, &logTime);
            
            crashFile << "[" << std::put_time(&logTm, "%H:%M:%S") << "] ";
            crashFile << "[" << GetLevelString(entry.level) << "] ";
            
            if (!entry.file.empty() && entry.line != -1) {
                size_t lastSlash = entry.file.find_last_of("/\\");
                std::string shortFile = (lastSlash == std::string::npos) 
                    ? entry.file 
                    : entry.file.substr(lastSlash + 1);
                crashFile << "[" << shortFile << ":" << entry.line << "] ";
            }
            
            crashFile << entry.message << std::endl;
        }
        
        crashFile.close();
        spdlog::critical("Crash log saved to: {}", filename.str());
    }

    /*!
    \brief Enables backtrace logging to capture recent messages before crashes
    \param n_messages Number of recent messages to keep in backtrace buffer
    */
    void EnableBacktrace(size_t n_messages = 100) {
        spdlog::enable_backtrace(n_messages);
        spdlog::info("Backtrace enabled with {} messages buffer", n_messages);
    }
    
    /*!
    \brief Dumps the backtrace buffer to current sinks (useful during crashes)
    */
    void DumpBacktrace() {
        spdlog::dump_backtrace();
        spdlog::critical("Backtrace dumped due to critical event");
    }
    
    /*!
    \brief Sets up error handler for spdlog internal errors
    */
    void SetupErrorHandler() {
        spdlog::set_error_handler([](const std::string& msg) {
            // Write to emergency file if regular logging fails
            std::ofstream emergency("emergency_spdlog_error.log", std::ios::app);
            if (emergency.is_open()) {
                auto now = std::chrono::system_clock::now();
                auto time_t = std::chrono::system_clock::to_time_t(now);
                std::tm tm;
                localtime_s(&tm, &time_t);
                
                emergency << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") 
                         << "] SPDLOG ERROR: " << msg << std::endl;
                emergency.close();
            }
        });
    }

    // Keep this for backwards compatibility but make it optional
    void EnableFileLogging(const std::string& logFilePath = "logs/session.log") {
        try {
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath, true);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%L%$] [%s:%#] %v");
            
            // Create logger with file sink and panel sink (no console)
            auto logger = std::make_shared<spdlog::logger>("file_and_panel", 
                std::initializer_list<spdlog::sink_ptr>{file_sink, panel_sink_});
            
            spdlog::set_default_logger(logger);
            spdlog::info("Session logging enabled - all logs will be written to: {}", logFilePath);
        } catch (const spdlog::spdlog_ex& ex) {
            spdlog::error("Failed to initialize file logging: {}", ex.what());
        }
    }

private:
    SpdLogger() {
        panel_sink_ = std::make_shared<panel_sink_mt>();
        
        // Create logger with ONLY the panel sink (no console output, no file output)
        auto logger = std::make_shared<spdlog::logger>("panel_only", 
            std::initializer_list<spdlog::sink_ptr>{panel_sink_});
        spdlog::set_default_logger(logger);
        
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%^%L%$] [%s:%#] %v");
        spdlog::set_level(spdlog::level::debug);
        
        // Enable crash-related features
        EnableBacktrace(100);  // Keep last 100 messages
        SetupErrorHandler();   // Handle spdlog internal errors
    }

    ~SpdLogger() = default;
    SpdLogger(const SpdLogger&) = delete;
    SpdLogger& operator=(const SpdLogger&) = delete;

    /*!
    \brief Signal handler for automatic crash detection
    \param signal The signal number that was raised
    */
    static void HandleCrashSignal(int signal) {
        auto& logger = GetInstance();
        
        std::string crashReason = "Signal " + std::to_string(signal) + " (";
        switch (signal) {
            case SIGABRT: crashReason += "SIGABRT - Abnormal termination"; break;
            case SIGFPE:  crashReason += "SIGFPE - Floating point exception"; break;
            case SIGILL:  crashReason += "SIGILL - Illegal instruction"; break;
            case SIGINT:  crashReason += "SIGINT - Interrupt signal"; break;
            case SIGSEGV: crashReason += "SIGSEGV - Segmentation fault"; break;
            case SIGTERM: crashReason += "SIGTERM - Termination request"; break;
            default:      crashReason += "Unknown signal"; break;
        }
        crashReason += ")";
        
        logger.SaveCrashLog(crashReason);
        exit(signal);
    }

#ifdef _WIN32
    /*!
    \brief Windows exception handler for automatic crash detection
    \param exceptionInfo Windows exception information
    \return Exception handling result
    */
    static LONG WINAPI HandleWindowsException(EXCEPTION_POINTERS* exceptionInfo) {
        auto& logger = GetInstance();
        
        std::stringstream crashReason;
        crashReason << "Windows Exception 0x" << std::hex << exceptionInfo->ExceptionRecord->ExceptionCode;
        
        // Add exception-specific info
        switch (exceptionInfo->ExceptionRecord->ExceptionCode) {
            case EXCEPTION_ACCESS_VIOLATION:
                crashReason << " (Access Violation)";
                break;
            case EXCEPTION_STACK_OVERFLOW:
                crashReason << " (Stack Overflow)";
                break;
            case EXCEPTION_INT_DIVIDE_BY_ZERO:
                crashReason << " (Division by Zero)";
                break;
            default:
                crashReason << " (Unknown Exception)";
                break;
        }
        
        logger.SaveCrashLog(crashReason.str());
        return EXCEPTION_EXECUTE_HANDLER;
    }
#endif

    /*!
    \brief Gathers system information for crash reports
    \return String containing system information
    */
    std::string GatherSystemInfo() const {
        std::stringstream info;
        
#ifdef _WIN32        
        // Memory info
        MEMORYSTATUSEX memInfo = {};
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);
        info << "Total RAM: " << (memInfo.ullTotalPhys / 1024 / 1024) << " MB" << std::endl;
        info << "Available RAM: " << (memInfo.ullAvailPhys / 1024 / 1024) << " MB" << std::endl;
        
        // Process memory usage
        PROCESS_MEMORY_COUNTERS pmc = {};
        GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
        info << "Process Memory Usage: " << (pmc.WorkingSetSize / 1024 / 1024) << " MB" << std::endl;
#else
        info << "Platform: Non-Windows" << std::endl;
#endif
        
        return info.str();
    }

    const char* GetLevelString(SpdLogLevel level) const {
        switch (level) {
        case SpdLogLevel::Debug: return "DEBUG";
        case SpdLogLevel::Info: return "INFO";
        case SpdLogLevel::Warning: return "WARNING";
        case SpdLogLevel::Error: return "ERROR";
        case SpdLogLevel::Critical: return "CRITICAL";
        default: return "UNKNOWN";
        }
    }

    std::shared_ptr<panel_sink_mt> panel_sink_;
    std::string m_crashLogPath = "crash_logs/";
    bool m_crashOnlyMode = false;
};

// Basic logging macros - use these for normal logging
#define SPD_DEBUG(...)   do { std::ostringstream oss; oss << __VA_ARGS__; \
    SpdLogger::GetInstance().Log(SpdLogLevel::Debug, oss.str(), __FILE__, __LINE__); } while(0)
#define SPD_INFO(...)    do { std::ostringstream oss; oss << __VA_ARGS__; \
    SpdLogger::GetInstance().Log(SpdLogLevel::Info, oss.str(), __FILE__, __LINE__); } while(0)
#define SPD_WARNING(...) do { std::ostringstream oss; oss << __VA_ARGS__; \
    SpdLogger::GetInstance().Log(SpdLogLevel::Warning, oss.str(), __FILE__, __LINE__); } while(0)
#define SPD_ERROR(...)   do { std::ostringstream oss; oss << __VA_ARGS__; \
    SpdLogger::GetInstance().Log(SpdLogLevel::Error, oss.str(), __FILE__, __LINE__); } while(0)
#define SPD_CRITICAL(...) do { std::ostringstream oss; oss << __VA_ARGS__; \
    SpdLogger::GetInstance().Log(SpdLogLevel::Critical, oss.str(), __FILE__, __LINE__); } while(0)

// Crash logging macros - use these for crash scenarios
#define SPD_CRASH_LOG(...) do { \
    std::ostringstream oss; oss << __VA_ARGS__; \
    SpdLogger::GetInstance().Log(SpdLogLevel::Critical, oss.str(), __FILE__, __LINE__); \
    SpdLogger::GetInstance().SaveCrashLog(oss.str()); \
} while(0)

#define SPD_FATAL_CRASH(...) do { \
    std::ostringstream oss; oss << __VA_ARGS__; \
    SpdLogger::GetInstance().Log(SpdLogLevel::Critical, oss.str(), __FILE__, __LINE__); \
    SpdLogger::GetInstance().SaveCrashLog(oss.str()); \
    std::abort(); \
} while(0)



#endif