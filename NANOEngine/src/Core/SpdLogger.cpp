#include "pch.h"
/*!
\file       SpdLogger.cpp
\author     Anson Teng
\date  9/9/2025
\brief      This file contains implementations for SpdLogger wrapper around spdlog library.
			Provides unified logging interface with ImGui panel integration and crash-only file logging.
Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/

#include "SpdLogger.hpp"
#include "SpdLoggerInternal.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <csignal>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")
#endif

void SpdLogger::SetMinLogLevel(SpdLogLevel level) {
	spdlog::set_level(static_cast<spdlog::level::level_enum>(static_cast<int>(level)));
}

void SpdLogger::Log(SpdLogLevel level, const std::string& message,
	const std::string& file, int line) {
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
		} else {
			spdlog::debug(message);
		}
		break;
	case SpdLogLevel::Info:
		if (line != -1) {
			spdlog::log(spdlog::source_loc{ filename.c_str(), line, "" },
				spdlog::level::info, message);
		} else {
			spdlog::info(message);
		}
		break;
	case SpdLogLevel::Warning:
		if (line != -1) {
			spdlog::log(spdlog::source_loc{ filename.c_str(), line, "" },
				spdlog::level::warn, message);
		} else {
			spdlog::warn(message);
		}
		break;
	case SpdLogLevel::Error:
		if (line != -1) {
			spdlog::log(spdlog::source_loc{ filename.c_str(), line, "" },
				spdlog::level::err, message);
		} else {
			spdlog::error(message);
		}
		break;
	case SpdLogLevel::Critical:
		if (line != -1) {
			spdlog::log(spdlog::source_loc{ filename.c_str(), line, "" },
				spdlog::level::critical, message);
		} else {
			spdlog::critical(message);
		}
		break;
	}
}

std::vector<SpdLogEntry> SpdLogger::GetLogEntries() const {
	std::vector<SpdLogEntry> entries;
	if (m_impl && m_impl->panel_sink) {
		m_impl->panel_sink->get_logs(entries);
	}
	return entries;
}

void SpdLogger::ClearLogEntries() {
	if (m_impl && m_impl->panel_sink) {
		m_impl->panel_sink->clear_logs();
	}
}

void SpdLogger::EnableCrashOnlyLogging(const std::string& crashLogPath) {
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

void SpdLogger::SetupAutomaticCrashDetection() {
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

void SpdLogger::SaveCrashLog(const std::string& crashReason) {
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

void SpdLogger::EnableBacktrace(size_t n_messages) {
	spdlog::enable_backtrace(n_messages);
	spdlog::info("Backtrace enabled with {} messages buffer", n_messages);
}

void SpdLogger::DumpBacktrace() {
	spdlog::dump_backtrace();
	spdlog::critical("Backtrace dumped due to critical event");
}

void SpdLogger::SetupErrorHandler() {
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

void SpdLogger::EnableFileLogging(const std::string& logFilePath) {
	try {
		// Create directory if it doesn't exist
		std::filesystem::path logPath(logFilePath);
		if (logPath.has_parent_path()) {
			std::filesystem::create_directories(logPath.parent_path());
		}

		auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath, true);
		file_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%L%$] [%s:%#] %v");

		// Get the current default logger and add the file sink to it
		auto current_logger = spdlog::default_logger();
		auto sinks = current_logger->sinks();
		sinks.push_back(file_sink);

		// Create a new logger with all sinks (panel + file)
		auto logger = std::make_shared<spdlog::logger>("multi_sink", sinks.begin(), sinks.end());
		logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%L%$] [%s:%#] %v");
		logger->set_level(spdlog::level::debug);

		spdlog::set_default_logger(logger);
		spdlog::info("Session logging enabled - all logs will be written to: {}", logFilePath);
	} catch (const spdlog::spdlog_ex& ex) {
		spdlog::error("Failed to initialize file logging: {}", ex.what());
	}
}

SpdLogger::SpdLogger() {
	m_impl = std::make_unique<SpdLoggerImpl>();
	m_impl->panel_sink = std::make_shared<panel_sink_mt>();

	// Create logger with ONLY panel sink (no console output)
	// Note: File logging will be added via EnableFileLogging() in Application::Init()
	auto logger = std::make_shared<spdlog::logger>("panel_only",
		std::initializer_list<spdlog::sink_ptr>{m_impl->panel_sink});
	spdlog::set_default_logger(logger);

	spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%^%L%$] [%s:%#] %v");
	spdlog::set_level(spdlog::level::debug);

	// Enable crash-related features
	EnableBacktrace(100);  // Keep last 100 messages
	SetupErrorHandler();   // Handle spdlog internal errors
}

SpdLogger::~SpdLogger() {
	// Destructor needs to be defined in the .cpp file where SpdLoggerImpl is complete
}

void SpdLogger::HandleCrashSignal(int signal) {
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
long __stdcall SpdLogger::HandleWindowsException(EXCEPTION_POINTERS* exceptionInfo) {
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

std::string SpdLogger::GatherSystemInfo() const {
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

const char* SpdLogger::GetLevelString(SpdLogLevel level) const {
	switch (level) {
	case SpdLogLevel::Debug: return "DEBUG";
	case SpdLogLevel::Info: return "INFO";
	case SpdLogLevel::Warning: return "WARNING";
	case SpdLogLevel::Error: return "ERROR";
	case SpdLogLevel::Critical: return "CRITICAL";
	default: return "UNKNOWN";
	}
}