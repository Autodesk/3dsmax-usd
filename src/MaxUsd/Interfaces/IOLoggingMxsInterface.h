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

#include <MaxUsd/Builders/SceneBuilderOptions.h>

namespace MAXUSD_NS_DEF {

class MaxUSDAPI IOLoggingMxsInterface
{

public:
    /**
     * \brief Constructor
     * \param options Scene builder options.
     */
    IOLoggingMxsInterface(SceneBuilderOptions* options);

    /**
     * \brief Sets the log level to the held builder options.
     * \param level The log level to set.
     */
    void SetLogLevel(int level);

    /**
     * \brief Returns the log level from the held options.
     * \return The log level.
     */
    int GetLogLevel();

    /**
     * \brief Sets the log path to the held builder options.
     * \param logPath The log path to set.
     */
    void SetLogPath(const std::wstring& logPath);

    /**
     * \brief Returns the log path from the held builder options.
     * \return The log path.
     */
    const wchar_t* GetLogPath();

private:
    SceneBuilderOptions* options;
};
} // namespace MAXUSD_NS_DEF