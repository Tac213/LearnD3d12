#pragma once

#include "log_manager.h"
#include <spdlog/fmt/ostr.h>

#define REGISTER_LOGGER(LOGGER_NAME) \
    ::learn_d3d12::LogManager::get_instance().register_logger(#LOGGER_NAME)

#define LOG(LOGGER_NAME, LOG_LEVEL, ...) \
    ::learn_d3d12::LogManager::get_instance().log(#LOGGER_NAME, spdlog::level::LOG_LEVEL, __VA_ARGS__)

#define LOG_DEBUG(LOGGER_NAME, ...) LOG(LOGGER_NAME, debug, __VA_ARGS__)

#define LOG_INFO(LOGGER_NAME, ...) LOG(LOGGER_NAME, info, __VA_ARGS__)

#define LOG_WARN(LOGGER_NAME, ...) LOG(LOGGER_NAME, warn, __VA_ARGS__)

#define LOG_ERROR(LOGGER_NAME, ...) LOG(LOGGER_NAME, err, __VA_ARGS__)

#define LOG_CRITICAL(LOGGER_NAME, ...) LOG(LOGGER_NAME, critical, __VA_ARGS__)

#define FLUSH_LOGGER(LOGGER_NAME) \
    ::learn_d3d12::LogManager::get_instance().flush_logger(#LOGGER_NAME)
