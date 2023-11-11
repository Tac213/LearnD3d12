#include "log_manager.h"
#include <chrono>
#include <format>
#ifdef _WIN32
#include <shlobj_core.h>
#endif

namespace learn_d3d12
{
    void LogManager::initialize()
    {
        _stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        _stdout_sink->set_level(spdlog::level::info);
#ifndef _WIN32
        _stdout_sink->set_color(spdlog::level::warn, _stdout_sink->yellow);
        _stdout_sink->set_color(spdlog::level::err, _stdout_sink->red);
#endif
        std::string log_path = _get_log_file_path(_log_file_prefix);
        _engine_file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path);
        register_logger("LearnD3d12", true);
    }

    void LogManager::finalize()
    {
        for (const auto& logger_name : _logger_names)
        {
            if (auto logger = spdlog::get(logger_name))
            {
                logger->flush();
            }
        }
        spdlog::drop_all();
        _logger_names.clear();
    }

    void LogManager::register_logger(const std::string& logger_name, bool b_save_file)
    {
        std::shared_ptr<spdlog::logger> logger = spdlog::get(logger_name);
        if (logger)
        {
            return;
        }
        std::vector<spdlog::sink_ptr> sinks {_stdout_sink};
        if (b_save_file && _engine_file_sink)
        {
            sinks.push_back(_engine_file_sink);
        }
        logger = std::make_shared<spdlog::logger>(logger_name, sinks.begin(), sinks.end());
        spdlog::register_logger(logger);
        _logger_names.push_back(logger_name);
    }

    std::string LogManager::_get_log_file_path(const std::string& prefix)
    {
        const auto now = std::chrono::system_clock::now();
        const auto* time_zone = std::chrono::current_zone();
        const auto local_time = time_zone->to_local(now);
        auto time_string = std::format("{:%Y%m%d_%H%M%S}", floor<std::chrono::seconds>(local_time));
        std::string log_file_path;
#ifdef _WIN32
        wchar_t* appdata = nullptr;
        if (SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &appdata) == S_OK)
        {
            char dest[MAX_PATH];
            size_t i;
            int err = wcstombs_s(&i, dest, MAX_PATH, appdata, MAX_PATH);
            if (err)
            {
                log_file_path = "logs/" + prefix + "_" + time_string + ".log";
            }
            else
            {
                log_file_path = dest;
                log_file_path.append("/LearnD3d12/logs/" + prefix + "_" + time_string + ".log");
            }
        }
        else
        {
            log_file_path = "logs/" + prefix + "_" + time_string + ".log";
        }
#else
        log_file_path = "logs/" + prefix + "_" + time_string + ".log";
#endif
        return log_file_path;
    }
}  // namespace learn_d3d12
