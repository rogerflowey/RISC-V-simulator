#include <iostream>
#include <sstream>
#include <stdexcept>
#include <regex>
#include <cassert>
#include <string>
#include <algorithm>
#include "utils/logger/logger.hpp"

// Simple test framework macros
#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        std::cerr << "ASSERTION FAILED: " << #condition << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::abort(); \
    }

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

#define ASSERT_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        std::cerr << "ASSERTION FAILED: Expected " << (expected) << " but got " << (actual) \
                  << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::abort(); \
    }

#define ASSERT_NE(expected, actual) \
    if ((expected) == (actual)) { \
        std::cerr << "ASSERTION FAILED: Expected " << (expected) << " to not equal " << (actual) \
                  << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::abort(); \
    }

#define ASSERT_STREQ(expected, actual) \
    if (std::string(expected) != std::string(actual)) { \
        std::cerr << "ASSERTION FAILED: Expected '" << (expected) << "' but got '" << (actual) \
                  << "' at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::abort(); \
    }

#define EXPECT_TRUE(condition) \
    if (!(condition)) { \
        std::cerr << "EXPECTATION FAILED: " << #condition << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        test_failed = true; \
    }

#define EXPECT_FALSE(condition) EXPECT_TRUE(!(condition))

#define EXPECT_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        std::cerr << "EXPECTATION FAILED: Expected " << (expected) << " but got " << (actual) \
                  << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        test_failed = true; \
    }

#define EXPECT_NE(expected, actual) \
    if ((expected) == (actual)) { \
        std::cerr << "EXPECTATION FAILED: Expected " << (expected) << " to not equal " << (actual) \
                  << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        test_failed = true; \
    }

#define EXPECT_STREQ(expected, actual) \
    if (std::string(expected) != std::string(actual)) { \
        std::cerr << "EXPECTATION FAILED: Expected '" << (expected) << "' but got '" << (actual) \
                  << "' at " << __FILE__ << ":" << __LINE__ << std::endl; \
        test_failed = true; \
    }

bool test_failed = false;
int tests_run = 0;
int tests_passed = 0;

#define RUN_TEST(test_func) \
    do { \
        test_failed = false; \
        tests_run++; \
        std::cout << "Running " << #test_func << "... "; \
        test_func(); \
        if (!test_failed) { \
            std::cout << "PASSED" << std::endl; \
            tests_passed++; \
        } else { \
            std::cout << "FAILED" << std::endl; \
        } \
    } while(0)

class LoggerTest {
public:
    void SetUp() {
        // Reset output stream for each test
        output.str("");
        output.clear();
    }

    std::ostringstream output;
};

// Test basic logger creation and configuration
void test_LoggerCreation() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::INFO);
    EXPECT_TRUE(true); // Logger should be created without issues
}

void test_SetLogLevel() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::INFO);
    logger.SetLevel(LogLevel::WARN);
    // Log level change should not crash
    EXPECT_TRUE(true);
}

// Test basic logging functionality
void test_InfoLogging() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::INFO);
    logger.Info("Test info message");
    
    std::string result = test.output.str();
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("[INFO]"), std::string::npos);
    EXPECT_NE(result.find("Test info message"), std::string::npos);
    EXPECT_NE(result.find("test_InfoLogging"), std::string::npos); // Should contain function name
}

void test_WarnLogging() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::INFO);
    logger.Warn("Test warning message");
    
    std::string result = test.output.str();
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("[WARN]"), std::string::npos);
    EXPECT_NE(result.find("Test warning message"), std::string::npos);
    EXPECT_NE(result.find("test_WarnLogging"), std::string::npos);
}

void test_ErrorLogging() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::INFO);
    auto exception = logger.Error("Test error message");
    
    // Check that error was logged
    std::string result = test.output.str();
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("[ERROR]"), std::string::npos);
    EXPECT_NE(result.find("Test error message"), std::string::npos);
    EXPECT_NE(result.find("test_ErrorLogging"), std::string::npos);
    
    // Check that exception is returned with proper message
    std::string exception_msg = exception.what();
    EXPECT_NE(exception_msg.find("test_ErrorLogging"), std::string::npos);
    EXPECT_NE(exception_msg.find("Test error message"), std::string::npos);
}

void test_ErrorLoggingWithCustomException() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::INFO);
    auto exception = logger.Error<std::invalid_argument>("Custom exception message");
    
    // Check that error was logged
    std::string result = test.output.str();
    EXPECT_NE(result.find("[ERROR]"), std::string::npos);
    EXPECT_NE(result.find("Custom exception message"), std::string::npos);
    
    // Check exception type and message
    std::string exception_msg = exception.what();
    EXPECT_NE(exception_msg.find("Custom exception message"), std::string::npos);
}

// Test log level filtering
void test_LogLevelFiltering_InfoLevel() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::INFO);
    
    logger.Info("Info message");
    logger.Warn("Warn message");
    auto error_ex = logger.Error("Error message");
    
    std::string result = test.output.str();
    EXPECT_NE(result.find("Info message"), std::string::npos);
    EXPECT_NE(result.find("Warn message"), std::string::npos);
    EXPECT_NE(result.find("Error message"), std::string::npos);
}

void test_LogLevelFiltering_WarnLevel() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::WARN);
    
    logger.Info("Info message");
    logger.Warn("Warn message");
    auto error_ex = logger.Error("Error message");
    
    std::string result = test.output.str();
    EXPECT_EQ(result.find("Info message"), std::string::npos); // Should be filtered out
    EXPECT_NE(result.find("Warn message"), std::string::npos);
    EXPECT_NE(result.find("Error message"), std::string::npos);
}

void test_LogLevelFiltering_ErrorLevel() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::ERROR);
    
    logger.Info("Info message");
    logger.Warn("Warn message");
    auto error_ex = logger.Error("Error message");
    
    std::string result = test.output.str();
    EXPECT_EQ(result.find("Info message"), std::string::npos); // Should be filtered out
    EXPECT_EQ(result.find("Warn message"), std::string::npos); // Should be filtered out
    EXPECT_NE(result.find("Error message"), std::string::npos);
}

// Test structured logging with LogEntry
void test_StructuredLogging_SingleField() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::INFO);
    
    logger.With("key1", "value1").Info("Structured message");
    
    std::string result = test.output.str();
    EXPECT_NE(result.find("Structured message"), std::string::npos);
    EXPECT_NE(result.find("key1=\"value1\""), std::string::npos);
}

void test_StructuredLogging_MultipleFields() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::INFO);
    
    logger.With("user", "john")
          .With("action", "login")
          .With("timestamp", 12345)
          .Info("User action logged");
    
    std::string result = test.output.str();
    EXPECT_NE(result.find("User action logged"), std::string::npos);
    EXPECT_NE(result.find("user=\"john\""), std::string::npos);
    EXPECT_NE(result.find("action=\"login\""), std::string::npos);
    EXPECT_NE(result.find("timestamp=\"12345\""), std::string::npos);
}

void test_StructuredLogging_DifferentTypes() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::INFO);
    
    logger.With("string_val", "test")
          .With("int_val", 42)
          .With("double_val", 3.14)
          .With("bool_val", true)
          .Info("Mixed types");
    
    std::string result = test.output.str();
    EXPECT_NE(result.find("string_val=\"test\""), std::string::npos);
    EXPECT_NE(result.find("int_val=\"42\""), std::string::npos);
    EXPECT_NE(result.find("double_val=\"3.14\""), std::string::npos);
    EXPECT_NE(result.find("bool_val=\"1\""), std::string::npos);
}

void test_StructuredLogging_Warn() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::INFO);
    
    logger.With("warning_code", 404).Warn("Resource not found");
    
    std::string result = test.output.str();
    EXPECT_NE(result.find("[WARN]"), std::string::npos);
    EXPECT_NE(result.find("Resource not found"), std::string::npos);
    EXPECT_NE(result.find("warning_code=\"404\""), std::string::npos);
}

void test_StructuredLogging_Error() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::INFO);
    
    auto exception = logger.With("error_code", 500)
                           .With("module", "database")
                           .Error("Connection failed");
    
    std::string result = test.output.str();
    EXPECT_NE(result.find("[ERROR]"), std::string::npos);
    EXPECT_NE(result.find("Connection failed"), std::string::npos);
    EXPECT_NE(result.find("error_code=\"500\""), std::string::npos);
    EXPECT_NE(result.find("module=\"database\""), std::string::npos);
    
    // Check exception message
    std::string exception_msg = exception.what();
    EXPECT_NE(exception_msg.find("Connection failed"), std::string::npos);
}

// Test log format components
void test_LogFormat_ContainsTimestamp() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::INFO);
    logger.Info("Test message");
    
    std::string result = test.output.str();
    // Should contain timestamp in YYYY-MM-DD HH:MM:SS format
    std::regex timestamp_regex(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})");
    EXPECT_TRUE(std::regex_search(result, timestamp_regex));
}

void test_LogFormat_ContainsSourceLocation() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::INFO);
    logger.Info("Test message");
    
    std::string result = test.output.str();
    // Should contain function name and source location
    EXPECT_NE(result.find("test_LogFormat_ContainsSourceLocation"), std::string::npos);
    EXPECT_NE(result.find("logger.cpp:"), std::string::npos);
}

void test_LogFormat_MessageInQuotes() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::INFO);
    logger.Info("Test message");
    
    std::string result = test.output.str();
    EXPECT_NE(result.find("\"Test message\""), std::string::npos);
}

// Test level_to_string function
void test_LevelToString() {
    EXPECT_STREQ(level_to_string(LogLevel::INFO), "INFO");
    EXPECT_STREQ(level_to_string(LogLevel::WARN), "WARN");
    EXPECT_STREQ(level_to_string(LogLevel::ERROR), "ERROR");
}

// Test global logger function
void test_GlobalLogger() {
    Logger& global_logger1 = logger();
    Logger& global_logger2 = logger();
    
    // Should return the same instance (singleton)
    EXPECT_EQ(&global_logger1, &global_logger2);
}

// Test edge cases and error conditions
void test_EmptyMessage() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::INFO);
    logger.Info("");
    
    std::string result = test.output.str();
    EXPECT_NE(result.find("\"\""), std::string::npos); // Empty message in quotes
}

void test_SpecialCharactersInMessage() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::INFO);
    logger.Info("Message with \"quotes\" and \n newlines");
    
    std::string result = test.output.str();
    EXPECT_FALSE(result.empty());
    // The message should be logged (exact formatting may vary)
}

void test_SpecialCharactersInFields() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::INFO);
    
    logger.With("key with spaces", "value \"with\" quotes")
          .Info("Special chars test");
    
    std::string result = test.output.str();
    EXPECT_FALSE(result.empty());
    // Should handle special characters gracefully
}

// Test LogEntry independent usage
void test_LogEntryChaining() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::INFO);
    
    // Test that LogEntry can be used with method chaining
    auto entry = logger.With("step", 1);
    entry.With("action", "start").Info("Process started");
    
    std::string result = test.output.str();
    EXPECT_NE(result.find("step=\"1\""), std::string::npos);
    EXPECT_NE(result.find("action=\"start\""), std::string::npos);
    EXPECT_NE(result.find("Process started"), std::string::npos);
}

// Performance-related tests
void test_FilteredLogsNoProcessing() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::ERROR);
    
    // These should be filtered out and not processed
    logger.Info("This should not appear");
    logger.Warn("This should not appear either");
    
    std::string result = test.output.str();
    EXPECT_TRUE(result.empty());
}

void test_MultipleMessagesFormatting() {
    LoggerTest test;
    test.SetUp();
    
    Logger logger(test.output, LogLevel::INFO);
    
    logger.Info("First message");
    logger.Warn("Second message");
    logger.With("id", 123).Info("Third message");
    
    std::string result = test.output.str();
    
    // Count the number of log entries (each should end with newline)
    size_t newline_count = std::count(result.begin(), result.end(), '\n');
    EXPECT_EQ(newline_count, 3);
    
    // All messages should be present
    EXPECT_NE(result.find("First message"), std::string::npos);
    EXPECT_NE(result.find("Second message"), std::string::npos);
    EXPECT_NE(result.find("Third message"), std::string::npos);
}

int main() {
    std::cout << "Running Logger Tests..." << std::endl;
    std::cout << "========================" << std::endl;
    
    // Run all tests
    RUN_TEST(test_LoggerCreation);
    RUN_TEST(test_SetLogLevel);
    RUN_TEST(test_InfoLogging);
    RUN_TEST(test_WarnLogging);
    RUN_TEST(test_ErrorLogging);
    RUN_TEST(test_ErrorLoggingWithCustomException);
    RUN_TEST(test_LogLevelFiltering_InfoLevel);
    RUN_TEST(test_LogLevelFiltering_WarnLevel);
    RUN_TEST(test_LogLevelFiltering_ErrorLevel);
    RUN_TEST(test_StructuredLogging_SingleField);
    RUN_TEST(test_StructuredLogging_MultipleFields);
    RUN_TEST(test_StructuredLogging_DifferentTypes);
    RUN_TEST(test_StructuredLogging_Warn);
    RUN_TEST(test_StructuredLogging_Error);
    RUN_TEST(test_LogFormat_ContainsTimestamp);
    RUN_TEST(test_LogFormat_ContainsSourceLocation);
    RUN_TEST(test_LogFormat_MessageInQuotes);
    RUN_TEST(test_LevelToString);
    RUN_TEST(test_GlobalLogger);
    RUN_TEST(test_EmptyMessage);
    RUN_TEST(test_SpecialCharactersInMessage);
    RUN_TEST(test_SpecialCharactersInFields);
    RUN_TEST(test_LogEntryChaining);
    RUN_TEST(test_FilteredLogsNoProcessing);
    RUN_TEST(test_MultipleMessagesFormatting);
    
    std::cout << std::endl;
    std::cout << "========================" << std::endl;
    std::cout << "Tests Results: " << tests_passed << "/" << tests_run << " passed" << std::endl;
    
    if (tests_passed == tests_run) {
        std::cout << "All tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << (tests_run - tests_passed) << " tests FAILED!" << std::endl;
        return 1;
    }
}
