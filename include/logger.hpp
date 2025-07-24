#pragma once

#include "utils/logger/logger.hpp"
#include "utils/clock.hpp"

inline Logger logger = Logger().WithContext("cycle",Clock::getInstance().getTime());