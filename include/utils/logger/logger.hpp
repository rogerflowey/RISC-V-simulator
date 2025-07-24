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
 * @class LogBuilder
 * @brief A temporary object to build a single log entry with ad-hoc fields, using a fluent API.
 *        (Formerly LogEntry)
 */
class LogBuilder {
private:
    Logger& logger;
    std::vector<std::pair<std::string, std::string>> ephemeral_fields;

public:
    inline LogBuilder(Logger& logger);

    // Adds a key-value field to this specific log entry.
    template<typename T>
    inline LogBuilder& With(const std::string& key, const T& value);

    // Terminal methods that write the log
    inline void Info(const std::string& message, const std::source_location& loc = std::source_location::current());
    inline void Warn(const std::string& message, const std::source_location& loc = std::source_location::current());
    template<typename ExceptionType = std::runtime_error>
    [[nodiscard]] inline ExceptionType Error(const std::string& message, const std::source_location& loc = std::source_location::current());
};

/**
 * @class Logger
 * @brief The main logger class. Manages configuration, context, and creates log entries.
 */
class Logger {
    friend class LogBuilder;

private:
    std::ostream& output_stream;
    LogLevel min_level;
    // Fields that are part of this logger's permanent context
    std::vector<std::pair<std::string, std::string>> context_fields;

    // Private constructor for creating new loggers with context
    inline Logger(
        std::ostream& stream,
        LogLevel level,
        std::vector<std::pair<std::string, std::string>> context)
        : output_stream(stream), min_level(level), context_fields(std::move(context)) {}

    // The core logging function, called by LogBuilder or Logger itself
    inline void log_internal(
        LogLevel level,
        const std::string& message,
        const std::vector<std::pair<std::string, std::string>>& ephemeral_fields,
        const std::source_location& loc);

public:
    inline Logger(std::ostream& stream = std::cout, LogLevel level = LogLevel::INFO);

    inline void SetLevel(LogLevel level);

    

    // --- Entry points for logging ---

    /**
     * @brief Creates a new logger with an added permanent context field.
     * @return A new Logger instance with the combined context.
     */
    template<typename T>
    [[nodiscard]] inline Logger WithContext(const std::string& key, const T& value) const;

    // Start a structured log with a temporary field for a single message
    template<typename T>
    inline LogBuilder With(const std::string& key, const T& value);

    // Log a simple message directly (will include the logger's context)
    inline void Info(const std::string& message, const std::source_location& loc = std::source_location::current());
    inline void Warn(const std::string& message, const std::source_location& loc = std::source_location::current());
    template<typename ExceptionType = std::runtime_error>
    [[nodiscard]] inline ExceptionType Error(const std::string& message, const std::source_location& loc = std::source_location::current());
};

// --- Helper Functions ---

inline const char* level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

// --- LogBuilder Implementation ---

inline LogBuilder::LogBuilder(Logger& logger) : logger(logger) {}

template<typename T>
inline LogBuilder& LogBuilder::With(const std::string& key, const T& value) {
    std::stringstream ss;
    ss << value;
    ephemeral_fields.emplace_back(key, ss.str());
    return *this; // Return reference to self for chaining
}

inline void LogBuilder::Info(const std::string& message, const std::source_location& loc) {
    logger.log_internal(LogLevel::INFO, message, ephemeral_fields, loc);
}

inline void LogBuilder::Warn(const std::string& message, const std::source_location& loc) {
    logger.log_internal(LogLevel::WARN, message, ephemeral_fields, loc);
}

template<typename ExceptionType>
[[nodiscard]] inline ExceptionType LogBuilder::Error(const std::string& message, const std::source_location& loc) {
    logger.log_internal(LogLevel::ERROR, message, ephemeral_fields, loc);
    std::string exception_message = std::string(loc.function_name()) + ": " + message;
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
    const std::vector<std::pair<std::string, std::string>>& ephemeral_fields,
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

    // First, print the permanent context fields from the logger
    for (const auto& pair : context_fields) {
        output_stream << pair.first << "=\"" << pair.second << "\" ";
    }
    // Then, print the temporary fields for this specific message
    for (const auto& pair : ephemeral_fields) {
        output_stream << pair.first << "=\"" << pair.second << "\" ";
    }

    output_stream << "(" << loc.function_name() << " @ " << loc.file_name() << ":" << loc.line() << ")\n";
}

template<typename T>
[[nodiscard]] inline Logger Logger::WithContext(const std::string& key, const T& value) const {
    // Create a copy of the current context
    auto new_context = this->context_fields;
    
    // Add the new field
    std::stringstream ss;
    ss << value;
    new_context.emplace_back(key, ss.str());

    // Return a new logger instance with the new, extended context
    return Logger(this->output_stream, this->min_level, std::move(new_context));
}

template<typename T>
inline LogBuilder Logger::With(const std::string& key, const T& value) {
    LogBuilder builder(*this);
    return builder.With(key, value);
}

inline void Logger::Info(const std::string& message, const std::source_location& loc) {
    // Pass an empty vector for ephemeral fields
    log_internal(LogLevel::INFO, message, {}, loc);
}

inline void Logger::Warn(const std::string& message, const std::source_location& loc) {
    log_internal(LogLevel::WARN, message, {}, loc);
}

template<typename ExceptionType>
[[nodiscard]] inline ExceptionType Logger::Error(const std::string& message, const std::source_location& loc) {
    log_internal(LogLevel::ERROR, message, {}, loc);
    std::string exception_message = std::string(loc.function_name()) + ": " + message;
    return ExceptionType(exception_message);
}