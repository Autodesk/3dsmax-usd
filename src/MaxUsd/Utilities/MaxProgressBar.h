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

#include <MaxUsd.h>
#include <chrono>
#include <maxapi.h>

namespace MAXUSD_NS_DEF {

/**
 * \brief Wrapper around the 3ds Max Progress Bar, offering convenience APIs to handle smooth UI and resource
 * acquisition and release.
 * \remarks This updates the 3ds Max at regular interval during the import of large USD files, in order to avoid giving
 * the impression that 3ds Max is throttled down by lack of resources or other processes. Note that
 * this does not update the 3ds Max progress bar outside of "tick notifications" to the wrapper
 * object at this time. Long-running tasks with few steps might instead benefit from extending from
 * this wrapper and schedule updates through other means.
 */
class MaxProgressBar
{
public:
    /**
     * \brief Constructor.
     * \param title Title to display on the progress bar.
     * \param total Total number of steps that the task will require (which will be presented as a percentage to the
     * User).
     * \param maxUpdateInterval Maximum duration (in milliseconds) between refreshes of the UI if the percentage has
     * not changed. This is done in order to avoid making it seems like 3ds Max's progress bar is
     * frozen.
     */
    MaxProgressBar(
        const std::wstring& title,
        size_t              total = 1,
        long long           maxUpdateInterval = DEFAULT_REFRESH_INTERVAL_IN_MS) noexcept;

    /**
     * \brief Destructor.
     * \remarks This perform an RAII-style check to stop the currently ongoing progress, if it was not already manually
     * stopped.
     */
    ~MaxProgressBar();

    /**
     * \brief Start the progress for the current task.
     */
    void Start();

    /**
     * \brief Stop the progress for the current task.
     * \param showPct Whether or not to display the percentage (100%)
     * \param msg Message to display in the progress bar.
     */
    void Stop(bool showPct = true, const std::wstring& msg = {});

    /**
     * \brief Update the progress bar to display the most recent status of the task to the User.
     * \param currentProgress The current step of the total overall progress.
     * \param stepName The current progress step's name, which will display in the progress bar.
     */
    void
    UpdateProgress(size_t currentProgress, bool showPct = true, const std::wstring& stepName = {});

    /**
     * \brief Sets the total number of steps that the task will require
     * \param total the Total number of steps.
     */
    void SetTotal(size_t total);

    /**
     * \brief Return a flag indicating if the Progress Bar is currently stopped.
     * \return A flag indicating if the Progress Bar is currently stopped.
     */
    bool IsStopped() const;

    /**
     * \brief Returns the currently set total number of steps (number of steps to completion).
     * \return Total steps.
     */
    size_t GetTotal();

    /**
     * \brief Set whether the progress bar is enabled or not.
     * If disabled, calls to start, stop, and update, have no effect. Enabling or disabling
     * mid-progress is not recommended as the max progress bar state may not be right.
     * \param enabled True if enabled.
     */
    void SetEnabled(bool enabled);

    /**
     * \brief Checks if the progress bar is enabled.
     * \return True if enabled.
     */
    bool IsEnabled();

protected:
    /// Type definition for the type of internal clock to use:
    using clock_t = std::chrono::high_resolution_clock;
    /// Type definition for the time signature to use:
    using timestamp_t = std::chrono::time_point<clock_t>;
    /// default progress bar refresh interval
    static const long long DEFAULT_REFRESH_INTERVAL_IN_MS = 2000;

protected:
    /// Title to display on the 3ds Max progress bar:
    std::wstring title;
    /// Total number of steps of the task:
    size_t total { 1 };
    /// Current progress percentage of the task:
    int progressPercentage { -1 };
    /// The current step's name.
    std::wstring currentStep;
    /// Flag indicating if the task is currently stopped:
    bool isStopped { true };
    /// Timestamp of the last progress bar update:
    timestamp_t lastUpdate { clock_t::now() };
    /// Maximum time interval between successive update (in milliseconds):
    long long maxUpdateInterval { 0 };
    /// Flag to enable disable the progress bar.
    bool enabled = true;
};

} // namespace MAXUSD_NS_DEF
