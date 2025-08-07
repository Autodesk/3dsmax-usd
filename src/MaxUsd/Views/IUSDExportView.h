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

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>

/**
 * \brief USD file import dialog.
 */
class IUSDExportView
{
public:
    /**
     * \brief Destructor.
     */
    virtual ~IUSDExportView() = 0 { }

    /**
     * \brief Display the View.
     * \return A flag indicating whether the User chose to import the USD file.
     */
    virtual bool Execute() = 0;

    /**
     * \brief Get the build configuration options for the file to export.
     * \return The build configuration options for the file to export.
     */
    virtual const MaxUsd::USDSceneBuilderOptions& GetBuildOptions() const = 0;
};
