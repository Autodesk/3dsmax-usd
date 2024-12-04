//
// Copyright 2022 Autodesk
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

#include <pxr/base/tf/declarePtrs.h>
#include <pxr/base/tf/registryManager.h>
#include <pxr/base/tf/singleton.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/tf/weakBase.h>
#include <pxr/base/vt/dictionary.h>
#include <pxr/pxr.h>

#include <string>

class QWidget;

PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_WEAK_PTRS(MaxUsdJobContextRegistry);

/// We provide macros that are entry points into the job context logic.
class MaxUsdJobContextRegistry : public TfWeakBase
{
public:
    /// An job context basically wraps a function that tweaks the set of import/export options. This
    /// job context has a name and UI components, as well as an enabler function that allows
    /// specifying the options dictionary.
    ///
    /// To register an export job context, you need to use the REGISTER_EXPORT_JOB_CONTEXT macro for
    /// each export job context supported by the library.
    ///
    /// In order for the core system to discover the plugin, you need a \c plugInfo.json that
    /// declares job contexts. \code
    /// {
    ///   "Plugins": [
    ///     {
    ///       "Info": {
    ///         "MaxUsd": {
    ///          "JobContextPlugin": {
    ///          }
    ///         }
    ///       },
    ///       "Name": "myUsdPlugin",
    ///       "LibraryPath": "../myUsdPlugin.dll",
    ///       "Type": "library"
    ///     }
    ///   ]
    /// }
    /// \endcode

    /// Enabler function, returns a dictionary containing all the options for the context.
    using EnablerFn = std::function<VtDictionary()>;
    using OptionsFn = std::function<VtDictionary(std::string, QWidget*, const VtDictionary&)>;

    /// Get all registered export job contexts:
    static TfTokenVector ListJobContexts() { return GetInstance()._ListJobContexts(); }

    /// All the information registered for a specific job context.
    struct ContextInfo
    {
        TfToken   jobContext;
        TfToken   niceName;
        TfToken   exportDescription;
        EnablerFn exportEnablerCallback;
        OptionsFn exportOptionsCallback;
        TfToken   importDescription;
        EnablerFn importEnablerCallback;
        OptionsFn importOptionsCallback;
    };

    /// Gets the conversion information associated with \p jobContext on export and import
    static const ContextInfo& GetJobContextInfo(const TfToken& jobContext)
    {
        return GetInstance()._GetJobContextInfo(jobContext);
    }

    /// Registers an export job context, with nice name, description and enabler function.
    ///
    /// \param jobContext name will be used directly as one of the valid values of the job context option.
    /// \param niceName is the name displayed in the options dialog.
    /// \param description is displayed as a tooltip in the  options dialog.
    /// \param enablerFct will be called after option parsing to enable context specific options.
    MaxUSDAPI void RegisterExportJobContext(
        const std::string& jobContext,
        const std::string& niceName,
        const std::string& description,
        EnablerFn          enablerFct,
        bool               fromPython = false);

    /// Registers an export job context ui option callback.
    ///
    /// \param jobContext the name of the registered job context.
    /// \param optionsFct will be called after the user hits the option button in the USD export UI.
    MaxUSDAPI void SetExportOptionsUI(
        const std::string& jobContext,
        OptionsFn          optionsFct,
        bool               fromPython = false);

    /// Registers an import job context, with nice name, description and enabler function.
    ///
    /// \param jobContext name will be used directly as one of the valid values of the job context option.
    /// \param niceName is the name displayed in the options dialog.
    /// \param description is displayed as a tooltip in the  options dialog.
    /// \param enablerFct will be called after option parsing to enable context specific options.
    MaxUSDAPI void RegisterImportJobContext(
        const std::string& jobContext,
        const std::string& niceName,
        const std::string& description,
        EnablerFn          enablerFct,
        bool               fromPython = false);

    /// Registers an import job context ui option callback.
    ///
    /// \param jobContext the name of the registered job context.
    /// \param optionsFct will be called after the user hits the option button in the USD import UI.
    MaxUSDAPI void SetImportOptionsUI(
        const std::string& jobContext,
        OptionsFn          optionsFct,
        bool               fromPython = false);

    MaxUSDAPI static MaxUsdJobContextRegistry& GetInstance();

private:
    MaxUSDAPI TfTokenVector      _ListJobContexts();
    MaxUSDAPI const ContextInfo& _GetJobContextInfo(const TfToken&);

    MaxUsdJobContextRegistry();
    ~MaxUsdJobContextRegistry();
    friend class TfSingleton<MaxUsdJobContextRegistry>;
};

#define REGISTER_EXPORT_JOB_CONTEXT(name, niceName, description, enablerFct) \
    TF_REGISTRY_FUNCTION(MaxUsdJobContextRegistry)                           \
    {                                                                        \
        MaxUsdJobContextRegistry::GetInstance().RegisterExportJobContext(    \
            name, niceName, description, enablerFct);                        \
    }

#define REGISTER_EXPORT_JOB_CONTEXT_FCT(name, niceName, description)         \
    static VtDictionary _ExportJobContextEnabler_##name();                   \
    TF_REGISTRY_FUNCTION(MaxUsdJobContextRegistry)                           \
    {                                                                        \
        MaxUsdJobContextRegistry::GetInstance().RegisterExportJobContext(    \
            #name, niceName, description, &_ExportJobContextEnabler_##name); \
    }                                                                        \
    VtDictionary _ExportJobContextEnabler_##name()

#define REGISTER_IMPORT_JOB_CONTEXT(name, niceName, description, enablerFct) \
    TF_REGISTRY_FUNCTION(MaxUsdJobContextRegistry)                           \
    {                                                                        \
        MaxUsdJobContextRegistry::GetInstance().RegisterImportJobContext(    \
            name, niceName, description, enablerFct);                        \
    }

#define REGISTER_IMPORT_JOB_CONTEXT_FCT(name, niceName, description)         \
    static VtDictionary _ImportJobContextEnabler_##name();                   \
    TF_REGISTRY_FUNCTION(MaxUsdJobContextRegistry)                           \
    {                                                                        \
        MaxUsdJobContextRegistry::GetInstance().RegisterImportJobContext(    \
            #name, niceName, description, &_ImportJobContextEnabler_##name); \
    }                                                                        \
    VtDictionary _ImportJobContextEnabler_##name()

PXR_NAMESPACE_CLOSE_SCOPE
