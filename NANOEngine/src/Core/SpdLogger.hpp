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

#define FMT_UNICODE 0  // Disable Unicode to avoid UTF-8 requirement

#include "NANOEngineAPI.hpp"
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <sstream>

#ifdef _WIN32
// Forward declarations for Windows types
struct _EXCEPTION_POINTERS;
typedef struct _EXCEPTION_POINTERS EXCEPTION_POINTERS;
typedef EXCEPTION_POINTERS* PEXCEPTION_POINTERS;
#endif

enum class SpdLogLevel {
    Debug = 0,
  Info = 1,
    Warning = 2,
    Error = 3,
    Critical = 4
};

struct SpdLogEntry {
    SpdLogLevel level{};
    std::string message{};
    std::string file{};
    int line{};
    std::chrono::system_clock::time_point timestamp{};
    std::string formattedMessage{};
};

// Opaque pointer to hide spdlog implementation details
struct SpdLoggerImpl;

// Suppress warning about DLL interface for STL types
#pragma warning(push)
#pragma warning(disable: 4251)  // 'identifier' : class 'type' needs to have dll-interface to be used by clients of class 'type2'

class NANOENGINE_API SpdLogger {
public:
    static SpdLogger& GetInstance() {
        static SpdLogger instance;
      return instance;
    }

    void SetMinLogLevel(SpdLogLevel level);

    void Log(SpdLogLevel level, const std::string& message,
   const std::string& file = "", int line = -1);

    std::vector<SpdLogEntry> GetLogEntries() const;

 void ClearLogEntries();

    /*!
    \brief Enables crash-only logging mode with automatic crash detection
    \param crashLogPath Directory to save crash logs (created if doesn't exist)
    */
    void EnableCrashOnlyLogging(const std::string& crashLogPath = "crash_logs/");

    /*!
    \brief Sets up automatic crash detection using signal handlers and Windows exceptions
    */
    void SetupAutomaticCrashDetection();

    /*!
    \brief Saves current logs to a crash file with timestamp and system information
    \param crashReason Description of what caused the crash
    */
    void SaveCrashLog(const std::string& crashReason = "Manual crash log requested");

    /*!
    \brief Enables backtrace logging to capture recent messages before crashes
    \param n_messages Number of recent messages to keep in backtrace buffer
    */
    void EnableBacktrace(size_t n_messages = 100);
    
    /*!
    \brief Dumps the backtrace buffer to current sinks (useful during crashes)
    */
    void DumpBacktrace();
 
    /*!
    \brief Sets up error handler for spdlog internal errors
    */
    void SetupErrorHandler();

    // Keep this for backwards compatibility but make it optional
    void EnableFileLogging(const std::string& logFilePath = "logs/session.log");

private:
    SpdLogger();
    ~SpdLogger();
    SpdLogger(const SpdLogger&) = delete;
    SpdLogger& operator=(const SpdLogger&) = delete;

    /*!
    \brief Signal handler for automatic crash detection
    \param signal The signal number that was raised
    */
    static void HandleCrashSignal(int signal);

#ifdef _WIN32
    /*!
    \brief Windows exception handler for automatic crash detection
    \param exceptionInfo Windows exception information
    \return Exception handling result
    */
    static long __stdcall HandleWindowsException(EXCEPTION_POINTERS* exceptionInfo);
#endif

    /*!
    \brief Gathers system information for crash reports
    \return String containing system information
    */
    std::string GatherSystemInfo() const;

  const char* GetLevelString(SpdLogLevel level) const;

    std::unique_ptr<SpdLoggerImpl> m_impl;
    std::string m_crashLogPath = "crash_logs/";
    bool m_crashOnlyMode = false;
};

#pragma warning(pop)

// Basic logging macros - use these for normal logging
#define SPD_DEBUG(...)   do { std::ostringstream oss; oss << __VA_ARGS__; \
    SpdLogger::GetInstance().Log(SpdLogLevel::Debug, oss.str(), __FILE__, __LINE__); } while(0)
#define SPD_INFO(...)  do { std::ostringstream oss; oss << __VA_ARGS__; \
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