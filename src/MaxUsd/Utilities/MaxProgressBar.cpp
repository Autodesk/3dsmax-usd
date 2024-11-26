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
#include "MaxProgressBar.h"

namespace MAXUSD_NS_DEF {

MaxProgressBar::MaxProgressBar(
    const std::wstring& title,
    size_t              total,
    long long           maxUpdateInterval) noexcept
    : title { title }
    , total { total }
    , maxUpdateInterval { maxUpdateInterval }
{
    // Nothing to do.
}

MaxProgressBar::~MaxProgressBar()
{
    if (!isStopped) {
        Stop();
    }
}

void MaxProgressBar::Start()
{
    if (!enabled) {
        return;
    }
    isStopped = false;
    GetCOREInterface()->ProgressStart(
        title.c_str(), TRUE, [](LPVOID arg) { return DWORD(0); }, NULL);
    UpdateProgress(0);
}

void MaxProgressBar::Stop(bool showPct, const std::wstring& msg)
{
    if (!enabled) {
        return;
    }
    UpdateProgress(total, false, msg);
    GetCOREInterface()->ProgressEnd();
    isStopped = true;
}

void MaxProgressBar::UpdateProgress(
    size_t              currentProgress,
    bool                showPct,
    const std::wstring& stepName)
{
    if (!enabled) {
        return;
    }
    if (!isStopped) {
        // Ensure the provided total number of steps for the task is meaningful for the context of a
        // progress bar. Since a total number of steps of "0" does not allow us to accurately inform
        // the User about the progress of a task, this falls back to display "0%" until the task is
        // complete.
        //
        // While this may not be ideal, this edge case is more meaningful than displaying "100%"
        // until the task completes, which may lead the User to believe that the task is complete
        // while it is actually still ongoing. Ideally, this may be better handled by an
        // "indeterminate" progress bar with no percentage information.
        int updatedProgressPercentage = 0;
        if (total != 0) {
            updatedProgressPercentage = int(currentProgress * 100 / total);
        }
        auto dt
            = std::chrono::duration_cast<std::chrono::milliseconds>(clock_t::now() - lastUpdate);

        // Prevent the 3ds Max UI from flickering by only updating the progress bar if the progress
        // percentage has changed, if the step's name has changed or if the maximum update interval
        // has been reached:
        if (this->currentStep != stepName || updatedProgressPercentage != progressPercentage
            || dt.count() >= maxUpdateInterval) {
            progressPercentage = updatedProgressPercentage;
            lastUpdate = clock_t::now();
            this->currentStep = stepName;
            GetCOREInterface()->ProgressUpdate(progressPercentage, showPct, stepName.c_str());
        }
    }
}

void MaxProgressBar::SetTotal(size_t total) { this->total = total; }

bool MaxProgressBar::IsStopped() const { return isStopped; }

size_t MaxProgressBar::GetTotal() { return total; }

void MaxProgressBar::SetEnabled(bool enabled) { this->enabled = enabled; }

bool MaxProgressBar::IsEnabled() { return enabled; }

} // namespace MAXUSD_NS_DEF
