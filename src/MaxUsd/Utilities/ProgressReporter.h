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
namespace MAXUSD_NS_DEF {

/**
 * \brief General purpose utility class for reporting progress. Callbacks to run
 * when starting, updating, and finishing progress are hooked up from the constructor.
 */
class ProgressReporter
{
public:
    ProgressReporter() { }

    ProgressReporter(
        const std::function<void(const std::wstring&)>& startFunction,
        const std::function<void(int)>&                 updateFunction,
        const std::function<void()>&                    endFunction)
        : start(startFunction)
        , update(updateFunction)
        , end(endFunction)
    {
    }

    void Start(const std::wstring& title) const
    {
        if (start) {
            start(title);
        }
    }

    void Update(int progress) const
    {
        if (update) {
            update(progress);
        }
    }

    void End() const
    {
        if (end) {
            end();
        }
    }

private:
    std::function<void(const std::wstring&)> start;
    std::function<void(int)>                 update;
    std::function<void()>                    end;
};
} // namespace MAXUSD_NS_DEF