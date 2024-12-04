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
#include "Logging.h"

#include "TranslationUtils.h"

#include <max.h>
#include <spdlog/common.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

namespace MAXUSD_NS_DEF {

std::shared_ptr<spdlog::logger> Log::spdLogger = nullptr;
bool                            Log::paused = false;

Log::Session::Session(const std::string& name, const Options& options)
{
    // If level == off, avoid to create a logger entirely, otherwise the file may still be created.
    if (options.level == Level::Off) {
        return;
    }
    try {
        spdLogger = spdlog::rotating_logger_st(name, options.path, maxLogSize, maxLogFiles);
    } catch (...) {
        GetCOREInterface()->Log()->LogEntry(
            SYSLOG_WARN,
            FALSE,
            nullptr,
            _T("The %s log could not be created at %s."),
            MaxUsd::UsdStringToMaxString(name),
            options.path.c_str());
        return;
    }

    spdLogger->set_error_handler(
        [](const std::string& logError) { spdLogger->error("Logger Error : {}", logError); });

    spdlog::level::level_enum level;
    switch (options.level) {
    case Level::Info: level = spdlog::level::info; break;
    case Level::Warn: level = spdlog::level::warn; break;
    case Level::Error: level = spdlog::level::err; break;
    case Level::Off:
    default: level = spdlog::level::off;
    }
    spdLogger->set_level(level);
}

Log::Session::~Session()
{
    if (spdLogger) {
        spdLogger->flush();
        // Spdlog "registers" the loggers internally upon creation.
        spdlog::drop(spdLogger->name());
        spdLogger.reset();
    }
}

void Log::Message(Level messageType, const std::wstring& message)
{
    switch (messageType) {
    case Level::Info: Info(message); break;
    case Level::Warn: Warn(message); break;
    case Level::Error: Error(message); break;
    default: break;
    }
}

void Log::Pause() { paused = true; }

void Log::Resume() { paused = false; }

} // namespace MAXUSD_NS_DEF
