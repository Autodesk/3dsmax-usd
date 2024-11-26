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
#include "USDCore.h"

#if MAX_VERSION_MAJOR < 26
#include <qdir.h>
#endif

#if MAX_VERSION_MAJOR == 24
#include <cctype> // force the correct std::tolower instead of the locale one
#endif

#include "Utilities/MaxSupportUtils.h"
#include "Utilities/TranslationUtils.h"

#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>

#include <notify.h>
#include <vector>

#define CREATE_DEBUG_CONSOLE 0

#if CREATE_DEBUG_CONSOLE
#include <fcntl.h>
#include <io.h>
#endif

void USDCore::initialize()
{
#if CREATE_DEBUG_CONSOLE
    if (AllocConsole()) {
        // Create stdin and stdout/err file handles
        auto hIn = CreateFile(L"CONIN$", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        auto hOut = CreateFile(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);

        // Do the same for win32 API
        SetStdHandle(STD_INPUT_HANDLE, hIn);
        SetStdHandle(STD_OUTPUT_HANDLE, hOut);
        SetStdHandle(STD_ERROR_HANDLE, hOut);

        // Check for open() like handles
        auto fdIn = _open_osfhandle(intptr_t(hIn), _O_RDONLY);
        auto fdOut = _open_osfhandle(intptr_t(hOut), _O_APPEND);
        _dup2(fdIn, 0);
        _dup2(fdOut, 1);
        _dup2(fdOut, 2);

        // Handle FILE* type handles
        freopen("CONIN$", "r", stdin);
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
#endif

    // WORKAROUND :
    // Force load the usdSkel and usdRender libraries instead of letting them be lazy-loaded.
    // Otherwise, if usd libraries are loaded/unloaded from third party code (ex: from arnold-usd),
    // we can get into situations where the USD static initalizers are executed more than
    // once, and this trips USD and can causes crashes. In principle this should not be happening,
    // it looks to me that something is wrong in the USD plugin loading code, with dependencies not
    // correctly being flagged as "loaded" in all cases. Indeed, it is also possible to trigger
    // crashes with usdSkel, by just force loading all registered plugins - if usdSkelImaging gets
    // loaded first, and loads the usdSkel.dll as a side effect, later, when we try to actually load
    // the usdSkel plugin, we get a crash because of some debug symbols being already defined.
    auto& plugRegistry = pxr::PlugRegistry::GetInstance();
    plugRegistry.GetPluginWithName("usdSkel")->Load();
    plugRegistry.GetPluginWithName("usdRender")->Load();

    // Avoid lazy loading some of the USD plugins we know we will likely need - which can cause
    // unacceptable delays when first using USD. Intentionally keeping the load calls above separate
    // because the intent is different. Reloading an already loaded plugin is a noop.
    std::vector<std::string> dependencies = { "sdf",
                                              "usdSkel",
                                              "usdUI",
                                              "hdSt",
                                              "hgiGL",
                                              "usd",
                                              "usdHydra",
                                              "ar",
                                              "usdVol",
                                              "usdMtlx",
                                              "glf",
                                              "hd",
                                              "sdrGlslfx",
                                              "usdVolImaging",
                                              "hdx",
                                              "hio",
                                              "usdSkelImaging",
                                              "ndr",
                                              "usdShade",
                                              "usdImagingGL",
                                              "usdGeom",
                                              "usdRi",
                                              "usdImaging",
                                              "usdRiImaging",
                                              "usdLux",
                                              "usdMedia",
                                              "usdRender",
                                              "usdPhysics",
                                              "usdShaders",
                                              "usdAbc",
                                              "hdStorm" };

    for (auto& dependency : dependencies) {
        auto plug = plugRegistry.GetPluginWithName(dependency);
        if (!plug) {
            continue;
        }
        plug->Load();
        DbgAssert(plug->IsLoaded());
    }

    RegisterNotification(
        MaxSDKSupport::DeletedModifierNotifyHandler, nullptr, NOTIFY_PRE_MODIFIER_DELETED);
    RegisterNotification(
        MaxSDKSupport::DeletedModifierNotifyHandler, nullptr, NOTIFY_POST_MODIFIER_DELETED);
}

fs::path USDCore::sanitizedFilename(const MCHAR* filePath, const MCHAR* defaultExtension)
{
    auto str = MaxUsd::MaxStringToUsdString(filePath);
    auto ext = MaxUsd::MaxStringToUsdString(defaultExtension);
    return sanitizedFilename(str, ext);
}

fs::path
USDCore::sanitizedFilename(const std::string& filePath, const std::string& defaultExtension)
{
    auto path = fs::path(filePath);
    path = path.make_preferred().string();
    if (path.has_extension()) {
        std::string ext = path.extension().string();
        std::transform(
            ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
        path.replace_extension(ext);
        if (!defaultExtension.empty()) {
            if (std::set<std::string> { ".usd", ".usdc", ".usda", ".usdz" }.count(ext) == 0) {
                {
                    return path.replace_extension(defaultExtension);
                }
            }
        }
    } else if (!path.empty() && !defaultExtension.empty()) {
        return path.string() + defaultExtension;
    }
    return path;
}

#if MAX_VERSION_MAJOR < 26
std::string USDCore::relativePath(const fs::path& path, const fs::path& relative_to)
{
    fs::path p = fs::absolute(path);
    fs::path r = fs::absolute(relative_to);

    QDir    relativeTo(r.string().c_str());
    QString relativePath = relativeTo.relativeFilePath(p.string().c_str());

    return relativePath.toStdString();
}
#endif