/*!
\file       SpdLogger.hpp
\author     Anson Teng
\date       9/9/2025
\brief      This file contains declarations for SpdLogger wrapper around spdlog library.
            Provides unified logging interface with ImGui panel integration and custom sink support.
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
                spdlog::log(spdlog::source_loc{ filename.c_str(), line, "" },
                    spdlog::level::debug, message);
            }
            else {
                spdlog::debug(message);
            }
            break;
        case SpdLogLevel::Info:
            if (line != -1) {
                spdlog::log(spdlog::source_loc{ filename.c_str(), line, "" },
                    spdlog::level::info, message);
            }
            else {
                spdlog::info(message);
            }
            break;
        case SpdLogLevel::Warning:
            if (line != -1) {
                spdlog::log(spdlog::source_loc{ filename.c_str(), line, "" },
                    spdlog::level::warn, message);
            }
            else {
                spdlog::warn(message);
            }
            break;
        case SpdLogLevel::Error:
            if (line != -1) {
                spdlog::log(spdlog::source_loc{ filename.c_str(), line, "" },
                    spdlog::level::err, message);
            }
            else {
                spdlog::error(message);
            }
            break;
        case SpdLogLevel::Critical:
            if (line != -1) {
                spdlog::log(spdlog::source_loc{ filename.c_str(), line, "" },
                    spdlog::level::critical, message);
            }
            else {
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

    void EnableFileLogging(const std::string& logFilePath = "logs/engine.log") {
        try {
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath, true);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%L%$] [%s:%#] %v");

            // Create logger with file sink and panel sink (no console)
            auto logger = std::make_shared<spdlog::logger>("file_and_panel",
                std::initializer_list<spdlog::sink_ptr>{file_sink, panel_sink_});

            spdlog::set_default_logger(logger);
        }
        catch (const spdlog::spdlog_ex& ex) {
            spdlog::error("Failed to initialize file logging: {}", ex.what());
        }
    }

private:
    SpdLogger() {
        panel_sink_ = std::make_shared<panel_sink_mt>();

        // Create logger with ONLY the panel sink (no console output)
        auto logger = std::make_shared<spdlog::logger>("panel_only",
            std::initializer_list<spdlog::sink_ptr>{panel_sink_});
        spdlog::set_default_logger(logger);

        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%^%L%$] [%s:%#] %v");

        spdlog::set_level(spdlog::level::debug);

        spdlog::flush_on(spdlog::level::debug);
    }

    ~SpdLogger() = default;
    SpdLogger(const SpdLogger&) = delete;
    SpdLogger& operator=(const SpdLogger&) = delete;

    std::shared_ptr<panel_sink_mt> panel_sink_;
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