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
#include "ShaderWriterRegistry.h"

#include "LastResortUSDPreviewSurfaceWriter.h"
#include "RegistryHelper.h"

#include <MaxUsd/DebugCodes.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/UsdImaging/UsdImaging/tokens.h>
#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/base/tf/debug.h>

#include <maxapi.h>
#include <unordered_map>
#include <utility>

// custom specialization of std::hash can be injected in namespace std
template <> struct std::hash<Class_ID>
{
    std::size_t operator()(Class_ID const& c) const noexcept
    {
        std::size_t h1 = std::hash<ulong> {}(c.PartA());
        std::size_t h2 = std::hash<ulong> {}(c.PartB());
        return h1 ^ (h2 << 1);
    }
};

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    _tokens,

    (MaxUsd)
        (ShaderWriter)
);
// clang-format on

namespace {
struct _RegistryEntry
{
    MaxUsdShaderWriterRegistry::ContextPredicateFn _pred;
    MaxUsdShaderWriterRegistry::WriterFactoryFn    _writer;
    MaxUsdShaderWriterRegistry::TargetAgnosticFn   _targetAgnosticFn;
    int                                            _index;
};

using _Registry = std::unordered_multimap<Class_ID, _RegistryEntry>;
static _Registry _reg;
static int       _indexCounter = 0;

_Registry::const_iterator
_Find(const Class_ID& maxClassID, const MaxUsd::USDSceneBuilderOptions& exportArgs)
{
    using ContextSupport = MaxUsdShaderWriter::ContextSupport;

    _Registry::const_iterator ret = _reg.cend();
    _Registry::const_iterator first, last;
    std::tie(first, last) = _reg.equal_range(maxClassID);
    while (first != last) {
        ContextSupport support = first->second._pred(exportArgs);
        if (support == ContextSupport::Supported) {
            ret = first;
            break;
        } else if (support == ContextSupport::Fallback && ret == _reg.end()) {
            ret = first;
        }
        ++first;
    }

    return ret;
}
} // namespace

/* static */
void MaxUsdShaderWriterRegistry::Register(
    const TfToken&                              maxClassName,
    ContextPredicateFn                          pred,
    MaxUsdShaderWriterRegistry::WriterFactoryFn fn,
    TargetAgnosticFn                            targetAgnosticFn,
    bool                                        fromPython)
{
    auto name = maxClassName.GetString();
    auto classList = GetCOREInterface()->GetDllDir().ClassDir().GetClassList(MATERIAL_CLASS_ID);
    int  n = classList->Count(ACC_ALL);
    for (int i = 0; i < n; i++) {
        ClassDesc* classDesc = (*classList)[i].CD();
        auto       className = MaxUsd::GetNonLocalizedClassName(classDesc);
        if (maxClassName == className) {
            Register(classDesc->ClassID(), pred, fn, targetAgnosticFn, fromPython);
            break;
        }
    }
}

/* static */
void MaxUsdShaderWriterRegistry::Register(
    const Class_ID&                             maxClassID,
    ContextPredicateFn                          pred,
    MaxUsdShaderWriterRegistry::WriterFactoryFn fn,
    TargetAgnosticFn                            targetAgnosticFn,
    bool                                        fromPython)
{
    int index = _indexCounter++;
    TF_DEBUG(PXR_MAXUSD_REGISTRY)
        .Msg(
            "Registering MaxUsdShaderWriter for 3ds Max ID (%ul,%ul) with index %d.\n",
            maxClassID.PartA(),
            maxClassID.PartB(),
            index);

    _reg.insert(std::make_pair(maxClassID, _RegistryEntry { pred, fn, targetAgnosticFn, index }));

    // The unloader uses the index to know which entry to erase when there are
    // more than one for the same maxClassID.
    MaxUsd_RegistryHelper::AddUnloader(
        [maxClassID, index]() {
            _Registry::const_iterator it, itEnd;
            std::tie(it, itEnd) = _reg.equal_range(maxClassID);
            for (; it != itEnd; ++it) {
                if (it->second._index == index) {
                    _reg.erase(it);
                    break;
                }
            }
        },
        fromPython);
}

/* static */
MaxUsdShaderWriterRegistry::WriterFactoryFn MaxUsdShaderWriterRegistry::Find(
    const Class_ID&                       maxClassID,
    const MaxUsd::USDSceneBuilderOptions& exportArgs)
{
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdShaderWriterRegistry>();

    _Registry::const_iterator it = _Find(maxClassID, exportArgs);

    if (it != _reg.end()) {
        return it->second._writer;
    }

    // Try adding more writers via plugin load:
    static const TfTokenVector SCOPE = { _tokens->MaxUsd, _tokens->ShaderWriter };
    MaxUsd_RegistryHelper::FindAndLoadMaxPlug(SCOPE, maxClassID, MATERIAL_CLASS_ID);

    it = _Find(maxClassID, exportArgs);

    if (it != _reg.end()) {
        return it->second._writer;
    }

    // No applicable shader writer was found. If UsdPreviewSurface is the target, use a dummy
    // material as last resort (only supports diffuse color).
    WriterFactoryFn lastResortWriter = nullptr;
    if (exportArgs.GetConvertMaterialsTo() == UsdImagingTokens->UsdPreviewSurface
        && exportArgs.GetUseLastResortUSDPreviewSurfaceWriter() == true) {
        lastResortWriter = [](Mtl*                   material,
                              const SdfPath&         usd_path,
                              MaxUsdWriteJobContext& job_ctx) {
            return std::make_shared<LastResortUSDPreviewSurfaceWriter>(material, usd_path, job_ctx);
        };
    }
    return lastResortWriter;
}

/*static */
std::vector<Class_ID> MaxUsdShaderWriterRegistry::GetAllTargetAgnosticMaterials()
{
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdShaderWriterRegistry>();
    std::vector<Class_ID> targetAgnosticMaterials;
    for (auto& entry : _reg) {
        auto targetAgnosticMaterialsForEntry = entry.second._targetAgnosticFn();
        if (targetAgnosticMaterialsForEntry) {
            targetAgnosticMaterials.push_back(entry.first);
        }
    }
    return targetAgnosticMaterials;
}

PXR_NAMESPACE_CLOSE_SCOPE
