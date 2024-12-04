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
#include "PluginUtils.h"
// USDComponentVersion.h need to be included after the max sdk to avoid redefinition of VERSION_INT.
#include "TranslationUtils.h"
#include "USDComponentVersion.h"

class DllDesc;

namespace MAXUSD_NS_DEF {

const std::string GetPluginVersion() { return USD_COMPONENT_VERSION_STRING; }

std::string GetPluginDisplayVersion()
{
    const std::string ver = GetPluginVersion();
    return "v" + ver;
}

std::string GenerateGUID()
{
    GUID guid;
    CoCreateGuid(&guid);

    wchar_t szGUID[64] = { 0 };
    StringFromGUID2(guid, szGUID, 64);

    return std::string(MaxUsd::MaxStringToUsdString(szGUID));
}

} // namespace MAXUSD_NS_DEF