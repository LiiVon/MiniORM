#include "../../include/miniorm/core/utils.hpp"

namespace miniorm
{
    namespace
    {
        std::vector<String> &string_cache_storage()
        {
            static thread_local std::vector<String> cache;
            return cache;
        }
    }

#if MINIORM_ENABLE_LOGGING
    void Logger::output_log(Level level, const String &msg)
    {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        char time_buffer[64];
        std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));

        std::cerr << "[" << time_buffer << "] ["
                  << level_to_string(level) << "] " << msg << std::endl;
    }

    StringView Logger::level_to_string(Level level)
    {
        switch (level)
        {
        case Level::Debug:
            return "DEBUG";
        case Level::Info:
            return "INFO";
        case Level::Warning:
            return "WARNING";
        case Level::Error:
            return "ERROR";
        case Level::Critical:
            return "CRITICAL";
        default:
            return "UNKNOWN";
        }
    }
#endif

    std::runtime_error ExceptionFactory::database_error(const StringView &operation,
                                                        const StringView &message,
                                                        int32 error_code)
    {
        return std::runtime_error(
            StringFormatter::format("Database error during {}: {} (code: {})",
                                    operation, message, error_code));
    }

    std::runtime_error ExceptionFactory::sql_syntax_error(const StringView &sql,
                                                          const StringView &pos)
    {
        if (pos.empty())
        {
            return std::runtime_error(
                StringFormatter::format("SQL syntax error in query: {}", sql));
        }

        return std::runtime_error(
            StringFormatter::format("SQL syntax error near '{}': {}", pos, sql));
    }

    StringView StringCache::get_cached(const StringView &str)
    {
        auto &cache = string_cache_storage();

        for (const auto &cached_str : cache)
        {
            if (cached_str == str)
            {
                return cached_str;
            }
        }

        cache.emplace_back(str);
        return cache.back();
    }

    void StringCache::clear_cache()
    {
        auto &cache = string_cache_storage();
        cache.clear();
    }
}
