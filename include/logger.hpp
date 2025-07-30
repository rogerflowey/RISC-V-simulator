#pragma once


#include "utils/logger/logger.hpp"
#include "utils/clock.hpp"
#include <string>



inline Logger logger = Logger(std::cerr, LogLevel::INFO)
    .WithContext("cycle", std::function([]() {
        // This code will be executed for every log message,
        // getting the most up-to-date time.
        return std::to_string(Clock::getInstance().getTime());
    }));