/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "common/logger.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace {
  void setGlobalPattern(spdlog::logger &logger) {
    logger.set_pattern("[%D %T][%^%l%$][%n] %v");
  }

  void setDebugPattern(spdlog::logger &logger) {
    spdlog::set_pattern("[%D %T.%F][%^%l%$][%n][th:%t] %v");
  }

  std::shared_ptr<spdlog::logger> createLogger(const std::string &tag,
                                               bool debug_mode = false) {
    auto logger = spdlog::stdout_color_mt(tag);
    if (debug_mode) {
      setDebugPattern(*logger);
    } else {
      setGlobalPattern(*logger);
    }
    return logger;
  }
}  // namespace

namespace kagome::common {
  void setLogLevel(LogLevel lvl) {
    spdlog::set_level(lvl);
  }

  Logger createLogger(const std::string &tag) {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    auto logger = spdlog::get(tag);
    if (logger == nullptr) {
      logger = ::createLogger(tag);
    }
    return logger;
  }
}  // namespace kagome::common
