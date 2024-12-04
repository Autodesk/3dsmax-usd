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

#include <MaxUsd/MaxUSDAPI.h>

#include <MaxUsd.h>

namespace MAXUSD_NS_DEF {

/*
 * /brief returns the max-usd plugin version as defined in PackageContents.xml
 */
MaxUSDAPI const std::string GetPluginVersion();

/*
 * /brief returns the user friendly max-usd plugin version
 */
MaxUSDAPI std::string GetPluginDisplayVersion();

/*
 * /brief Creates and return a new GUID
 * \return new GUID as a std::string
 */
MaxUSDAPI std::string GenerateGUID();
} // namespace MAXUSD_NS_DEF