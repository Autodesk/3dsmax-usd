//
// Copyright 2016 Pixar
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
// © 2022 Autodesk, Inc. All rights reserved.
//
#include "RegistryHelper.h"

#include <MaxUsd/DebugCodes.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/base/js/converter.h>
#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/base/tf/debug.h>
#include <pxr/base/tf/scriptModuleLoader.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/tf/stl.h>
#include <pxr/base/tf/stringUtils.h>

#include <boost/python/import.hpp>
#include <map>
#include <maxapi.h>
#include <maxtypes.h>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
	_tokens,

	(providesTranslator)
);
// clang-format on

template <typename T> bool _GetData(const JsValue& any, T* val)
{
    if (!any.Is<T>()) {
        TF_CODING_ERROR("bad plugInfo.json");
        return false;
    }

    *val = any.Get<T>();
    return true;
}

template <typename T> bool _GetData(const JsValue& any, std::vector<T>* val)
{
    if (!any.IsArrayOf<T>()) {
        TF_CODING_ERROR("bad plugInfo.json");
        return false;
    }

    *val = any.GetArrayOf<T>();
    return true;
}

static bool _ReadNestedDict(const JsObject& data, const std::vector<TfToken>& keys, JsObject* dict)
{
    JsObject currDict = data;
    TF_FOR_ALL(iter, keys)
    {
        const TfToken& currKey = *iter;
        JsValue        any;
        if (!TfMapLookup(currDict, currKey, &any)) {
            return false;
        }

        if (!any.IsObject()) {
            TF_CODING_ERROR("bad plugInfo data.");
            return false;
        }
        currDict = any.GetJsObject();
    }
    *dict = currDict;
    return true;
}

static bool _ProvidesForType(
    const PlugPluginPtr&        plug,
    const std::vector<TfToken>& scope,
    const std::string&          typeName)
{
    JsObject metadata = plug->GetMetadata();
    JsObject maxTranslatorMetadata;
    if (!_ReadNestedDict(metadata, scope, &maxTranslatorMetadata)) {
        return false;
    }

    JsValue any;
    if (!TfMapLookup(maxTranslatorMetadata, _tokens->providesTranslator, &any)) {
        return false;
    }

    std::vector<std::string> translatedTypes;
    if (!_GetData(any, &translatedTypes)) {
        return false;
    }

    bool provides = std::find(translatedTypes.begin(), translatedTypes.end(), typeName)
        != translatedTypes.end();
    return provides;
}

static bool _IsSpecificToScope(const PlugPluginPtr& plug, const std::vector<TfToken>& scope)
{
    JsObject metadata = plug->GetMetadata();
    JsObject maxTranslatorMetadata;
    if (!_ReadNestedDict(metadata, scope, &maxTranslatorMetadata)) {
        return false;
    }

    return true;
}

/* static */
std::string _PluginDictScopeToDebugString(const std::vector<TfToken>& scope)
{
    std::vector<std::string> s;
    TF_FOR_ALL(iter, scope) { s.push_back(iter->GetString()); }
    return TfStringJoin(s, "/");
}

/* static */
void MaxUsd_RegistryHelper::FindAndLoadMaxPlug(
    const std::vector<TfToken>& scope,
    const Class_ID&             maxClassID,
    const SClass_ID&            superClassID)
{
    // convert 3ds Max Class ID to to 3ds Max Class Names
    auto classDesc = GetCOREInterface()->GetDllDir().ClassDir().FindClass(superClassID, maxClassID);
    if (!classDesc) {
        // the provided ClassID was not properly registered
        return;
    }
    std::string className = MaxUsd::GetNonLocalizedClassName(classDesc);

    return FindAndLoadMaxPlug(scope, className);
}

/* static */
void MaxUsd_RegistryHelper::FindAndLoadMaxPlug(
    const std::vector<TfToken>& scope,
    const std::string&          usdTypeName)
{
    PlugPluginPtrVector plugins = PlugRegistry::GetInstance().GetAllPlugins();
    TF_FOR_ALL(plugIter, plugins)
    {
        PlugPluginPtr plug = *plugIter;
        if (_ProvidesForType(plug, scope, usdTypeName)) {
            // load what we found so far
            {
                TF_DEBUG(PXR_MAXUSD_REGISTRY)
                    .Msg(
                        "Found %s MaxUsd plugin %s: %s = %s.\n",
                        plug->IsLoaded() ? "loaded" : "unloaded",
                        plug->GetName().c_str(),
                        _PluginDictScopeToDebugString(scope).c_str(),
                        usdTypeName.c_str());
                // Make sure that the Plug plugin is loaded to ensure that the
                // library is loaded in case it is a "library" type plugin with
                // no accompanying Max plugin. This is a noop if the plugin is
                // already loaded.
                plug->Load();
            }
            // Continue search. For shaders, we can have multiple importers and exporters for the
            // same Max node. A PhysicalMaterial can be exported as UsdPreviewSurface, MaterialX,
            // Arnold, PRman...
        }
    }
}

/* static */
void MaxUsd_RegistryHelper::FindAndLoadMaxUsdPlugs(const std::vector<TfToken>& scope)
{
    auto scopeString = [scope]() {
        std::string str;
        for (const auto& item : scope) {
            str += item;
            str += ":";
        }
        str.pop_back();
        return str;
    };

    PlugPluginPtrVector plugins = PlugRegistry::GetInstance().GetAllPlugins();
    TF_FOR_ALL(plugIter, plugins)
    {
        PlugPluginPtr plug = *plugIter;
        if (_IsSpecificToScope(plug, scope)) {
            {
                TF_DEBUG(PXR_MAXUSD_REGISTRY)
                    .Msg(
                        "Found %s plugin %s: Loading via USD API.\n",
                        scopeString().c_str(),
                        plug->GetName().c_str());
                // This is a noop if the plugin is already loaded.
                plug->Load();
            }
        }
    }
}

// Vector of Python unloaders
std::vector<std::function<void()>> g_pythonUnloaders;

static void PythonUnload(size_t unloaderIndex)
{
    g_pythonUnloaders[unloaderIndex]();
    // No destruction to keep the order, there should not be a lot of elements
}

/* static */
void MaxUsd_RegistryHelper::AddUnloader(const std::function<void()>& func, bool fromPython)
{
    if (fromPython) {
        g_pythonUnloaders.emplace_back(func);
        if (boost::python::import("atexit")
                .attr("register")(&PythonUnload, g_pythonUnloaders.size() - 1)
                .is_none()) {
            TF_CODING_ERROR("Couldn't register unloader to atexit");
        }
        return;
    }

    if (TfRegistryManager::GetInstance().AddFunctionForUnload(func)) {
        // It is likely that the registering plugin library is opened/closed
        // by Max and not via TfDlopen/TfDlclose. This means that the
        // unloaders won't be invoked unless we use RunUnloadersAtExit(),
        // which allows unloaders to be called from normal dlclose().
        TfRegistryManager::GetInstance().RunUnloadersAtExit();
    } else {
        TF_CODING_ERROR("Couldn't add unload function (was this function called from "
                        "outside a TF_REGISTRY_FUNCTION block?)");
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
