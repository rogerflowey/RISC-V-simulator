// utils/logger/logger.hpp
#pragma once

// This is the main control flag. If defined, all logging is compiled out.
// You can define this in your build system, e.g., g++ -DDISABLE_LOGGING ...
#ifdef DISABLE_LOGGING

#include <string>
#include <stdexcept>
#include <functional>
#include <iostream>         // <--- FIX: Included to use std::ostream
#include <source_location>  // <--- FIX: Included to match method signatures

// --- Dummy No-Op Logger Implementation (with corrected interface) ---

enum class LogLevel { INFO, WARN, ERROR };

class Logger; // Forward declaration

class LogBuilder {
public:
    inline LogBuilder(Logger&) {}
    template<typename T>
    inline LogBuilder& With(const std::string&, const T&) { return *this; }

    // --- FIX: Signatures now match the full implementation ---
    inline void Info(const std::string&, const std::source_location& = std::source_location::current()) {}
    inline void Warn(const std::string&, const std::source_location& = std::source_location::current()) {}
    template<typename ExceptionType = std::runtime_error>
    [[nodiscard]] inline ExceptionType Error(const std::string& message, const std::source_location& = std::source_location::current()) {
        return ExceptionType(message);
    }
};

class Logger {
public:
    // --- FIX: Constructor signature now matches the full implementation ---
    inline Logger(std::ostream& /* stream */ = std::cout, LogLevel /* level */ = LogLevel::INFO) {}

    inline void SetLevel(LogLevel) {}
    // --- FIX: SetStream signature now matches the full implementation ---
    inline void SetStream(std::ostream&) {}

    // --- FIX: Signatures now match the full implementation ---
    template<typename T>
    [[nodiscard]] inline Logger WithContext(const std::string&, const T&) const { return *this; }
    [[nodiscard]] inline Logger WithContext(const std::string&, std::function<std::string()>) const { return *this; }

    template<typename T>
    inline LogBuilder With(const std::string&, const T&) { return LogBuilder(*this); }

    // --- FIX: Signatures now match the full implementation ---
    inline void Info(const std::string&, const std::source_location& = std::source_location::current()) {}
    inline void Warn(const std::string&, const std::source_location& = std::source_location::current()) {}
    template<typename ExceptionType = std::runtime_error>
    [[nodiscard]] inline ExceptionType Error(const std::string& message, const std::source_location& = std::source_location::current()) {
        return ExceptionType(message);
    }
};


#else // --- The original, full-featured logger implementation (UNCHANGED) ---

#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <functional>
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
    std::ostream* output_stream;
    LogLevel min_level;
    std::vector<std::pair<std::string, std::function<std::string()>>> context_fields;

    inline Logger(
        std::ostream& stream,
        LogLevel level,
        std::vector<std::pair<std::string, std::function<std::string()>>> context)
        : output_stream(&stream), min_level(level), context_fields(std::move(context)) {}

    // The core logging function, called by LogBuilder or Logger itself
    inline void log_internal(
        LogLevel level,
        const std::string& message,
        const std::vector<std::pair<std::string, std::string>>& ephemeral_fields,
        const std::source_location& loc);

public:
    inline Logger(std::ostream& stream = std::cout, LogLevel level = LogLevel::INFO);

    inline void SetLevel(LogLevel level);
    inline void SetStream(std::ostream& stream);

    template<typename T>
    [[nodiscard]] inline Logger WithContext(const std::string& key, const T& value) const;
    [[nodiscard]] inline Logger WithContext(const std::string& key, std::function<std::string()> value_producer) const;

    template<typename T>
    inline LogBuilder With(const std::string& key, const T& value);

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
    return *this;
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
    : output_stream(&stream), min_level(level) {}

inline void Logger::SetLevel(LogLevel level) {
    min_level = level;
}

inline void Logger::SetStream(std::ostream& stream) {
    output_stream = &stream;
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
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    
    *output_stream << " [" << level_to_string(level) << "] "
                   << "\"" << message << "\" ";

    for (const auto& pair : context_fields) {
        *output_stream << pair.first << "=\"" << pair.second() << "\" ";
    }
    for (const auto& pair : ephemeral_fields) {
        *output_stream << pair.first << "=\"" << pair.second << "\" ";
    }

    *output_stream << "(" << loc.function_name() << " @ " << loc.file_name() << ":" << loc.line() << ")\n";
}

template<typename T>
[[nodiscard]] inline Logger Logger::WithContext(const std::string& key, const T& value) const {
    std::stringstream ss;
    ss << value;
    std::string static_value = ss.str();
    auto value_producer = [static_value]() { return static_value; };
    return this->WithContext(key, value_producer);
}

[[nodiscard]] inline Logger Logger::WithContext(const std::string& key, std::function<std::string()> value_producer) const {
    auto new_context = this->context_fields;
    new_context.emplace_back(key, std::move(value_producer));
    return Logger(*this->output_stream, this->min_level, std::move(new_context));
}

template<typename T>
inline LogBuilder Logger::With(const std::string& key, const T& value) {
    LogBuilder builder(*this);
    return builder.With(key, value);
}

inline void Logger::Info(const std::string& message, const std::source_location& loc) {
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

#endif // --- End of #ifdef DISABLE_LOGGING ---