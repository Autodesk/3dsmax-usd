//
// Copyright 2026 Autodesk
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
#include <MaxUsdObjects/MaxUsdObjectsAPI.h>

#include <AdskUsdComponentCreator/Host.h>
#include <memory>

class MaxUSDObjectsAPI MaxHost : public AdskUsdComponentCreator::Host
{
public:
    static std::shared_ptr<MaxHost> Instance();

    std::vector<pxr::SdfLayerRefPtr>
    GetSessionLayersForFilename(const std::string& filename) override;

    void HandleRenamedStage(
        const std::string& oldRootLayerFilename,
        const std::string& newRootLayerFilename) override;

    bool ExecuteWithUndo(
        const std::shared_ptr<AdskUsdComponentCreator::ComponentCommand>& command) override;
    bool HasSceneSelection() const override;
    bool AddSelectionToComponent(
        const AdskUsdComponentCreator::ComponentDescription::Ptr& componentDesc,
        const AdskUsdComponentCreator::VariantSelections&         variantSelections,
        bool                                                      isReplacing,
        const std::string&                                        purpose) override;
    bool   IsPinned() const override;
    bool   SetPinned(bool isPinned) override;
    double GetUiScaleFactor() const override;
    double ScaleForUi(double value) const override;
    void   TrackLayerChanges(const pxr::SdfLayerRefPtr& layer) override;
};
