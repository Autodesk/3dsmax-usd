//
// Copyright 2023 Autodesk
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#pragma once

#include <spdlog/spdlog.h>

// MAXX-63363: VS2019 v142: <experimental/filesystem> is deprecated and superseded by the C++17
// <filesystem> header
#if _MSVC_LANG > 201402L
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

#include <MaxUsd/MaxUSDAPI.h>

#include <MaxUsd.h>

#pragma warning(push)
#pragma warning(disable : 4251) // class 'boost::shared_ptr<T>' needs to have dll-interface to be
                                // used by clients of class MaxUsd::Log

namespace MAXUSD_NS_DEF {

/// Simple wrapper for some basic logging functionality provided by the spdlog library.
class MaxUSDAPI Log
{

    /// The current spd logger in use. Can be null.
    static std::shared_ptr<spdlog::logger> spdLogger;

    /// Max file size for logs.
    static const size_t maxLogSize = 1048576 * 200;
    /// How many rotating log files.
    static const size_t maxLogFiles = 5;

    static bool paused;

public:
    /**
     * \brief logging severity level for filtering
     * \comment value of enum is important as it reflects the index in the ui of qcombobox
     */
    enum class Level
    {
        Off = 0,
        Error = 1,
        Warn = 2,
        Info = 3
    };

    struct Options
    {
        fs::path path;
        Level    level = Level::Off;
    };

    /// The Session class is useful for setting up logging within a scope, and using RAII to destroy
    /// the logger. Starting a logging session will close any previously active sessions.
    class Session
    {
    public:
        Session(const std::string& name, const Options& options);
        ~Session();
    };

    static void Message(Level messageType, const std::wstring& message);

    static void Pause();
    static void Resume();

    template <typename FormatString, typename... Args>
    static void Warn(const FormatString& fmt, const Args&... args)
    {
        if (spdLogger && !paused) {
            spdLogger->warn(fmt, args...);
        }
    }

    template <typename FormatString, typename... Args>
    static void Info(const FormatString& fmt, const Args&... args)
    {
        if (spdLogger && !paused) {
            spdLogger->info(fmt, args...);
        }
    }

    template <typename FormatString, typename... Args>
    static void Error(const FormatString& fmt, const Args&... args)
    {
        if (spdLogger && !paused) {
            spdLogger->error(fmt, args...);
        }
    }
};

} // namespace MAXUSD_NS_DEF

#pragma warning(pop)
