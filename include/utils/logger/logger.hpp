// logger.hpp
#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <source_location> // C++20 required

// Represents the severity of the log message
enum class LogLevel {
    INFO,
    WARN,
    ERROR
};

// Forward declaration for the main logger class
class Logger;

/**
 * @class LogEntry
 * @brief A temporary object to build a single log entry using a fluent API.
 */
class LogEntry {
private:
    Logger& logger;
    std::vector<std::pair<std::string, std::string>> fields;

public:
    inline LogEntry(Logger& logger);

    // Adds a key-value field to the log entry's context.
    template<typename T>
    inline LogEntry& With(const std::string& key, const T& value);

    // Terminal methods that write the log
    inline void Info(const std::string& message, const std::source_location& loc = std::source_location::current());
    inline void Warn(const std::string& message, const std::source_location& loc = std::source_location::current());
    template<typename ExceptionType = std::runtime_error>
    [[nodiscard]] inline ExceptionType Error(const std::string& message, const std::source_location& loc = std::source_location::current());
};
/**
 * @class Logger
 * @brief The main logger class. Manages configuration and creates log entries.
 */
class Logger {
    friend class LogEntry;

private:
    std::ostream& output_stream;
    LogLevel min_level;

    // The core logging function, called by LogEntry
    inline void log_internal(
        LogLevel level,
        const std::string& message,
        const std::vector<std::pair<std::string, std::string>>& fields,
        const std::source_location& loc);

public:
    inline Logger(std::ostream& stream = std::cout, LogLevel level = LogLevel::INFO);

    inline void SetLevel(LogLevel level);

    // --- Entry points for logging ---

    // Start a structured log with a field
    template<typename T>
    inline LogEntry With(const std::string& key, const T& value);

    // Log a simple message directly
    inline void Info(const std::string& message, const std::source_location& loc = std::source_location::current());
    inline void Warn(const std::string& message, const std::source_location& loc = std::source_location::current());
    template<typename ExceptionType = std::runtime_error>
    [[nodiscard]] inline ExceptionType Error(const std::string& message, const std::source_location& loc = std::source_location::current());
};

// --- Helper Functions ---

// Converts LogLevel enum to a string. Inlined to keep it in the header.
inline const char* level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

// --- LogEntry Implementation ---

inline LogEntry::LogEntry(Logger& logger) : logger(logger) {}

template<typename T>
inline LogEntry& LogEntry::With(const std::string& key, const T& value) {
    std::stringstream ss;
    ss << value;
    fields.emplace_back(key, ss.str());
    return *this; // Return reference to self for chaining
}

inline void LogEntry::Info(const std::string& message, const std::source_location& loc) {
    logger.log_internal(LogLevel::INFO, message, fields, loc);
}

inline void LogEntry::Warn(const std::string& message, const std::source_location& loc) {
    logger.log_internal(LogLevel::WARN, message, fields, loc);
}

template<typename ExceptionType>
[[nodiscard]] inline ExceptionType LogEntry::Error(const std::string& message, const std::source_location& loc) {
    // Step 1: Log the detailed, structured error message.
    logger.log_internal(LogLevel::ERROR, message, fields, loc);

    // Step 2: Create a clean message for the exception itself.
    std::string exception_message = std::string(loc.function_name()) + ": " + message;

    // Step 3: Return the exception object for the caller to throw.
    return ExceptionType(exception_message);
}

// --- Logger Implementation ---

inline Logger::Logger(std::ostream& stream, LogLevel level)
    : output_stream(stream), min_level(level) {}

inline void Logger::SetLevel(LogLevel level) {
    min_level = level;
}

inline void Logger::log_internal(
    LogLevel level,
    const std::string& message,
    const std::vector<std::pair<std::string, std::string>>& fields,
    const std::source_location& loc)
{
    if (level < min_level) {
        return;
    }
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    output_stream << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
                  << " [" << level_to_string(level) << "] "
                  << "\"" << message << "\" ";

    for (const auto& pair : fields) {
        output_stream << pair.first << "=\"" << pair.second << "\" ";
    }

    output_stream << "(" << loc.function_name() << " @ " << loc.file_name() << ":" << loc.line() << ")\n";
}

template<typename T>
inline LogEntry Logger::With(const std::string& key, const T& value) {
    LogEntry entry(*this);
    return entry.With(key, value);
}

inline void Logger::Info(const std::string& message, const std::source_location& loc) {
    log_internal(LogLevel::INFO, message, {}, loc);
}

inline void Logger::Warn(const std::string& message, const std::source_location& loc) {
    log_internal(LogLevel::WARN, message, {}, loc);
}

template<typename ExceptionType>
[[nodiscard]] inline ExceptionType Logger::Error(const std::string& message, const std::source_location& loc) {
    // Create a temporary LogEntry to do the work, ensuring consistent behavior
    LogEntry entry(*this);
    return entry.Error<ExceptionType>(message, loc);
}

inline Logger& logger() {
    static Logger global_logger;
    return global_logger;
}
