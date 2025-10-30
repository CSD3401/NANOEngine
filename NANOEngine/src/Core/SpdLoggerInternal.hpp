/*!
\file       SpdLoggerInternal.hpp
\author Anson Teng
\date       9/9/2025
\brief      Internal implementation details for SpdLogger - contains spdlog dependencies.
            This header should only be included by SpdLogger.cpp, never by public headers.
Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/

#ifndef SPDLOGGER_INTERNAL_HPP
#define SPDLOGGER_INTERNAL_HPP

#include "SpdLogger.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/base_sink.h>
#include <deque>
#include <mutex>

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
   
     // Map spdlog levels to our SpdLogLevel enum
        // spdlog: trace=0, debug=1, info=2, warn=3, err=4, critical=5
      // ours:   Debug=0, Info=1, Warning=2, Error=3, Critical=4
        switch (msg.level) {
      case spdlog::level::trace:
      case spdlog::level::debug:
     entry.level = SpdLogLevel::Debug;
            break;
          case spdlog::level::info:
  entry.level = SpdLogLevel::Info;
 break;
            case spdlog::level::warn:
entry.level = SpdLogLevel::Warning;
       break;
            case spdlog::level::err:
     entry.level = SpdLogLevel::Error;
           break;
        case spdlog::level::critical:
            case spdlog::level::off:
     entry.level = SpdLogLevel::Critical;
     break;
 default:
       entry.level = SpdLogLevel::Info;
       break;
        }
        
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

// Implementation struct to hide spdlog dependencies
struct SpdLoggerImpl {
    std::shared_ptr<panel_sink_mt> panel_sink;
};

#endif // SPDLOGGER_INTERNAL_HPP
