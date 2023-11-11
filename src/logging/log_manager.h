#pragma once

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace learn_d3d12
{
    class LogManager
    {
    public:
        static LogManager& get_instance()
        {
            static LogManager instance;
            return instance;
        }
        ~LogManager() = default;
        LogManager(const LogManager&) = delete;
        LogManager(LogManager&&) = delete;
        LogManager& operator=(const LogManager&) = delete;
        LogManager& operator=(LogManager&&) = delete;

        void initialize();
        void finalize();
        void register_logger(const std::string& logger_name, bool save_file = true);

        template<typename... TARGS>
        void log(const std::string& logger_name, spdlog::level::level_enum level, fmt::format_string<TARGS...> fmt, TARGS&&... args)
        {
            std::shared_ptr<spdlog::logger> logger = spdlog::get(logger_name);
            if (!logger)
            {
                return;
            }
            switch (level)
            {
                case spdlog::level::trace:
                    logger->trace(fmt, std::forward<TARGS>(args)...);
                    break;
                case spdlog::level::debug:
                    logger->debug(fmt, std::forward<TARGS>(args)...);
                    break;
                case spdlog::level::info:
                    logger->info(fmt, std::forward<TARGS>(args)...);
                    break;
                case spdlog::level::warn:
                    logger->warn(fmt, std::forward<TARGS>(args)...);
                    break;
                case spdlog::level::err:
                    logger->error(fmt, std::forward<TARGS>(args)...);
                    break;
                case spdlog::level::critical:
                    logger->critical(fmt, std::forward<TARGS>(args)...);
                    break;
                default:
                    break;
            }
        }

        static void flush_logger(const std::string& logger_name)
        {
            std::shared_ptr<spdlog::logger> logger = spdlog::get(logger_name);
            if (!logger)
            {
                return;
            }
            logger->flush();
        }

    private:
        LogManager() = default;
        std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> _stdout_sink;
        std::shared_ptr<spdlog::sinks::basic_file_sink_mt> _engine_file_sink;
        std::string _log_file_prefix = "LearnD3d12";
        std::vector<std::string> _logger_names;

        static std::string _get_log_file_path(const std::string& prefix);
    };
}  // namespace learn_d3d12
