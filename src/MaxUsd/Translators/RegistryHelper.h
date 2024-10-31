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
#pragma once

#include <pxr/base/tf/token.h>
#include <pxr/pxr.h>

#include <functional>
#include <maxtypes.h>

PXR_NAMESPACE_OPEN_SCOPE

/// private helper so that both reader/writer registries can share the same
/// plugin discovery/load mechanism.
struct MaxUsd_RegistryHelper
{
    /// searches plugInfo's for 'value' at the specified 'scope'.
    ///
    /// The scope are the nested keys to search through in the plugInfo (for
    /// example, ["MaxUsd", "ShaderWriter"].
    ///
    /// {
    ///   'MaxUsd': {
    ///     'ShaderWriter': {
    ///       'providesTranslator': [
    ///          "PhysicalMaterial",
    ///          ...
    ///       ],
    ///     }
    ///   }
    /// }
    ///
    static void FindAndLoadMaxPlug(
        const std::vector<TfToken>& scope,
        const Class_ID&             value,
        const SClass_ID&            superClassID);
    static void
    FindAndLoadMaxPlug(const std::vector<TfToken>& scope, const std::string& usdTypeName);

    /// Searches the plugInfos and looks for plugins specified scope.
    /// (scope example below)
    ///
    /// "MaxUsd" : {
    ///     "PrimWriter" : {}
    /// }
    static void FindAndLoadMaxUsdPlugs(const std::vector<TfToken>& scope);

    static void AddUnloader(const std::function<void()>& func, bool fromPython = false);
};

/// SFINAE utility class to detect the presence of a CanExport static function
/// inside a writer class. Used by the registration macro for basic writers.
template <typename T> class HasCanExport
{
    typedef char _One;
    struct _Two
    {
        char _x[2];
    };

    template <typename C> static _One _Test(decltype(&C::CanExport));
    template <typename C> static _Two _Test(...);

public:
    enum
    {
        value = sizeof(_Test<T>(0)) == sizeof(char)
    };
};

/// SFINAE utility class to detect the presence of a IsMaterialTargetAgnostic static function
/// inside a writer class. Used by the registration macro for basic writers.
template <typename T> class HasIsMaterialTargetAgnostic
{
    typedef char _One;
    struct _Two
    {
        char _x[2];
    };

    template <typename C> static _One _Test(decltype(&C::IsMaterialTargetAgnostic));
    template <typename C> static _Two _Test(...);

public:
    enum
    {
        value = sizeof(_Test<T>(0)) == sizeof(char)
    };
};

// Function template that returns a pointer to IsMaterialTargetAgnostic if it exists
template <typename T>
auto IsMaterialTargetAgnosticFunc() ->
    typename std::enable_if<HasIsMaterialTargetAgnostic<T>::value, std::function<bool()>>::type
{
    return &T::IsMaterialTargetAgnostic;
}

// Function template that returns a lambda if IsMaterialTargetAgnostic doesn't exist
template <typename T>
auto IsMaterialTargetAgnosticFunc() ->
    typename std::enable_if<!HasIsMaterialTargetAgnostic<T>::value, std::function<bool()>>::type
{
    return []() -> bool { return false; };
}

PXR_NAMESPACE_CLOSE_SCOPE
