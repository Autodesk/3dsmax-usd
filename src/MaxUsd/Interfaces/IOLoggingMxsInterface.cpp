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
#include "IOLoggingMxsInterface.h"

#include <MaxUsd/Utilities/TranslationUtils.h>

#include <maxscript/kernel/exceptions.h>

namespace MAXUSD_NS_DEF {

IOLoggingMxsInterface::IOLoggingMxsInterface(SceneBuilderOptions* options)
{
    this->options = options;
}

void IOLoggingMxsInterface::SetLogLevel(int value)
{
    const auto level = MaxUsd::Log::Level(value);
    // Make sure the value is valid.
    if (level != MaxUsd::Log::Level::Off && level != MaxUsd::Log::Level::Error
        && level != MaxUsd::Log::Level::Warn && level != MaxUsd::Log::Level::Info) {
        WStr errorMsg(
            L"Incorrect LogLevel value. Accepted values are #off, #error, #warn and #info");
        throw RuntimeError(errorMsg.data());
    }
    options->SetLogLevel(level);
}

int IOLoggingMxsInterface::GetLogLevel() { return static_cast<int>(options->GetLogLevel()); }

void IOLoggingMxsInterface::SetLogPath(const std::wstring& logPath)
{
    if (!MaxUsd::IsValidAbsolutePath(logPath)) {
        throw RuntimeError(
            (logPath
             + std::wstring(L" is not a valid log path. The log path should be an absolute path "
                            L"with a maximum of 260 characters legal characters."))
                .c_str());
    }
    options->SetLogPath(logPath);
}

const wchar_t* IOLoggingMxsInterface::GetLogPath() { return options->GetLogPath().c_str(); }
} // namespace MAXUSD_NS_DEF