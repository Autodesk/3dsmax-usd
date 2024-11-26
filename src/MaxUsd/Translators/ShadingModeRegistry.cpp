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
#include "ShadingModeRegistry.h"

#include "RegistryHelper.h"
#include "ShadingModeExporter.h"
#include "ShadingModeExporterContext.h"

#include <pxr/base/tf/instantiateSingleton.h>
#include <pxr/base/tf/registryManager.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/tf/token.h>

#include <map>
#include <string>
#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(MaxUsdShadingModeTokens, PXR_MAXUSD_SHADINGMODE_TOKENS);

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    _tokens,

    (MaxUsd)
        (ShadingModePlugin)
);
// clang-format on

namespace {
static const std::string   _kEmptyString;
static const TfTokenVector PLUGIN_SCOPE = { _tokens->MaxUsd, _tokens->ShadingModePlugin };
} // namespace

struct _ExportShadingMode
{
    std::string                      _niceName;
    std::string                      _description;
    MaxUsdShadingModeExporterCreator _fn;
};
using _ExportRegistry = std::map<TfToken, _ExportShadingMode>;
static _ExportRegistry _exportReg;

TF_DEFINE_PUBLIC_TOKENS(MaxUsdPreferredMaterialTokens, PXR_MAXUSD_SHADINGCONVERSION_TOKENS);

using _MaterialConversionRegistry = std::map<TfToken, MaxUsdShadingModeRegistry::ConversionInfo>;
static _MaterialConversionRegistry _conversionReg;

bool MaxUsdShadingModeRegistry::RegisterExporter(
    const std::string&               name,
    std::string                      niceName,
    std::string                      description,
    MaxUsdShadingModeExporterCreator fn)
{
    const TfToken                                    nameToken(name);
    std::pair<_ExportRegistry::const_iterator, bool> insertStatus
        = _exportReg.insert(_ExportRegistry::value_type(
            nameToken, _ExportShadingMode { std::move(niceName), std::move(description), fn }));
    if (insertStatus.second) {
        MaxUsd_RegistryHelper::AddUnloader([nameToken]() { _exportReg.erase(nameToken); });
    } else {
        TF_CODING_ERROR("Multiple shading exporters named '%s'", name.c_str());
    }
    return insertStatus.second;
}

MaxUsdShadingModeExporterCreator MaxUsdShadingModeRegistry::_GetExporter(const TfToken& name)
{
    MaxUsd_RegistryHelper::FindAndLoadMaxUsdPlugs(PLUGIN_SCOPE);
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdShadingModeExportContext>();
    const auto it = _exportReg.find(name);
    return it == _exportReg.end() ? nullptr : it->second._fn;
}

const std::string& MaxUsdShadingModeRegistry::_GetExporterNiceName(const TfToken& name)
{
    MaxUsd_RegistryHelper::FindAndLoadMaxUsdPlugs(PLUGIN_SCOPE);
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdShadingModeExportContext>();
    const auto it = _exportReg.find(name);
    return it == _exportReg.end() ? _kEmptyString : it->second._niceName;
}

const std::string& MaxUsdShadingModeRegistry::_GetExporterDescription(const TfToken& name)
{
    MaxUsd_RegistryHelper::FindAndLoadMaxUsdPlugs(PLUGIN_SCOPE);
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdShadingModeExportContext>();
    const auto it = _exportReg.find(name);
    return it == _exportReg.end() ? _kEmptyString : it->second._description;
}

struct _ImportShadingMode
{
    std::string               _niceName;
    std::string               _description;
    MaxUsdShadingModeImporter _fn;
};
using _ImportRegistry = std::map<TfToken, _ImportShadingMode>;
static _ImportRegistry _importReg;

bool MaxUsdShadingModeRegistry::RegisterImporter(
    const std::string&        name,
    std::string               niceName,
    std::string               description,
    MaxUsdShadingModeImporter fn)
{
    const TfToken                                    nameToken(name);
    std::pair<_ImportRegistry::const_iterator, bool> insertStatus
        = _importReg.insert(_ImportRegistry::value_type(
            nameToken, _ImportShadingMode { std::move(niceName), std::move(description), fn }));
    if (insertStatus.second) {
        MaxUsd_RegistryHelper::AddUnloader([nameToken]() { _importReg.erase(nameToken); });
    } else {
        TF_CODING_ERROR("Multiple shading importers named '%s'", name.c_str());
    }
    return insertStatus.second;
}

MaxUsdShadingModeImporter MaxUsdShadingModeRegistry::_GetImporter(const TfToken& name)
{
    MaxUsd_RegistryHelper::FindAndLoadMaxUsdPlugs(PLUGIN_SCOPE);
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdShadingModeImportContext>();
    const auto it = _importReg.find(name);
    return it == _importReg.end() ? nullptr : it->second._fn;
}

const std::string& MaxUsdShadingModeRegistry::_GetImporterNiceName(const TfToken& name)
{
    MaxUsd_RegistryHelper::FindAndLoadMaxUsdPlugs(PLUGIN_SCOPE);
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdShadingModeImportContext>();
    const auto it = _importReg.find(name);
    return it == _importReg.end() ? _kEmptyString : it->second._niceName;
}

const std::string& MaxUsdShadingModeRegistry::_GetImporterDescription(const TfToken& name)
{
    MaxUsd_RegistryHelper::FindAndLoadMaxUsdPlugs(PLUGIN_SCOPE);
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdShadingModeImportContext>();
    const auto it = _importReg.find(name);
    return it == _importReg.end() ? _kEmptyString : it->second._description;
}

TfTokenVector MaxUsdShadingModeRegistry::_ListExporters()
{
    MaxUsd_RegistryHelper::FindAndLoadMaxUsdPlugs(PLUGIN_SCOPE);
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdShadingModeExportContext>();
    TfTokenVector ret;
    ret.reserve(_exportReg.size());
    for (const auto& e : _exportReg) {
        ret.push_back(e.first);
    }
    return ret;
}

TfTokenVector MaxUsdShadingModeRegistry::_ListImporters()
{
    MaxUsd_RegistryHelper::FindAndLoadMaxUsdPlugs(PLUGIN_SCOPE);
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdShadingModeImportContext>();
    TfTokenVector ret;
    ret.reserve(_importReg.size());
    for (const auto& e : _importReg) {
        ret.push_back(e.first);
    }
    return ret;
}

void MaxUsdShadingModeRegistry::RegisterExportConversion(
    const TfToken& materialConversion,
    const TfToken& renderContext,
    const TfToken& niceName,
    const TfToken& description)
{
    // It is perfectly valid to register the same material conversion more than once,
    // especially if exporters for a conversion are split across multiple libraries.
    // We will keep the first niceName registered.
    _MaterialConversionRegistry::iterator insertionPoint;
    bool                                  hasInserted;
    std::tie(insertionPoint, hasInserted)
        = _conversionReg.insert(_MaterialConversionRegistry::value_type(
            materialConversion,
            ConversionInfo { renderContext, niceName, description, TfToken(), true, false }));
    if (!hasInserted) {
        // Update the info:
        if (insertionPoint->second.exportDescription.IsEmpty()) {
            insertionPoint->second.exportDescription = description;
        }
        insertionPoint->second.hasExporter = true;
    }
}

void MaxUsdShadingModeRegistry::RegisterImportConversion(
    const TfToken& materialConversion,
    const TfToken& renderContext,
    const TfToken& niceName,
    const TfToken& description)
{
    // It is perfectly valid to register the same material conversion more than once,
    // especially if importers for a conversion are split across multiple libraries.
    // We will keep the first niceName registered.
    MaxUsdShadingModeRegistry::ConversionInfo conversionInfo(
        renderContext, niceName, TfToken(), description, false, true);
    _MaterialConversionRegistry::iterator insertionPoint;
    bool                                  hasInserted;
    std::tie(insertionPoint, hasInserted) = _conversionReg.insert(
        _MaterialConversionRegistry::value_type(materialConversion, conversionInfo));
    if (!hasInserted) {
        // Update the info:
        if (insertionPoint->second.importDescription.IsEmpty()) {
            insertionPoint->second.importDescription = description;
        }
        insertionPoint->second.hasImporter = true;
    }
}

TfTokenVector MaxUsdShadingModeRegistry::_ListMaterialConversions()
{
    MaxUsd_RegistryHelper::FindAndLoadMaxUsdPlugs(PLUGIN_SCOPE);
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdShadingModeExportContext>();
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdShadingModeImportContext>();
    TfTokenVector ret;
    ret.reserve(_conversionReg.size());
    for (const auto& e : _conversionReg) {
        ret.push_back(e.first);
    }
    return ret;
}

const MaxUsdShadingModeRegistry::ConversionInfo&
MaxUsdShadingModeRegistry::_GetMaterialConversionInfo(const TfToken& materialConversion)
{
    MaxUsd_RegistryHelper::FindAndLoadMaxUsdPlugs(PLUGIN_SCOPE);
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdShadingModeExportContext>();
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdShadingModeImportContext>();
    auto                        it = _conversionReg.find(materialConversion);
    static const ConversionInfo _emptyInfo;
    return it != _conversionReg.end() ? it->second : _emptyInfo;
}

TF_INSTANTIATE_SINGLETON(MaxUsdShadingModeRegistry);

MaxUsdShadingModeRegistry& MaxUsdShadingModeRegistry::GetInstance()
{
    return TfSingleton<MaxUsdShadingModeRegistry>::GetInstance();
}

MaxUsdShadingModeRegistry::MaxUsdShadingModeRegistry() { }

MaxUsdShadingModeRegistry::~MaxUsdShadingModeRegistry() { }

PXR_NAMESPACE_CLOSE_SCOPE
