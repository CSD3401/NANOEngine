#ifndef SPDLOG_LOGGER_HPP
#define SPDLOG_LOGGER_HPP

#ifdef APIENTRY
#undef APIENTRY
#endif

#define FMT_UNICODE 0  // Disable Unicode to avoid UTF-8 requirement

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <sstream>

enum class SpdLogLevel {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Critical = 4
};

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
        
        // Extract filename from full path
        std::string filename;
        if (!file.empty()) {
            size_t lastSlash = file.find_last_of("/\\");
            filename = (lastSlash == std::string::npos) ? file : file.substr(lastSlash + 1);
        }

        // Use spdlog with file and line info
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

    // Add file logging capability
    void EnableFileLogging(const std::string& logFilePath = "logs/engine.log") {
        try {
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath, true);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%L%$] [%s:%#] %v");
            
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%L%$] [%s:%#] %v");
            
            auto logger = std::make_shared<spdlog::logger>("multi_sink", 
                std::initializer_list<spdlog::sink_ptr>{console_sink, file_sink});
            
            spdlog::set_default_logger(logger);
        } catch (const spdlog::spdlog_ex& ex) {
            spdlog::error("Failed to initialize file logging: {}", ex.what());
        }
    }

private:
    SpdLogger() {
        // Initialize spdlog with colored console output
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto logger = std::make_shared<spdlog::logger>("console", console_sink);
        spdlog::set_default_logger(logger);
        
        // Set pattern to match your original format
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%^%L%$] [%s:%#] %v");
        
        // Set default level to debug
        spdlog::set_level(spdlog::level::debug);
        
        // Flush logs immediately for real-time debugging
        spdlog::flush_on(spdlog::level::debug);
    }

    ~SpdLogger() = default;
    SpdLogger(const SpdLogger&) = delete;
    SpdLogger& operator=(const SpdLogger&) = delete;
};

// Helper macros for easy logging with spdlog
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

#endif