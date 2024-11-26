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
#include "UsdStageRenderSettingsRollup.h"

#include "Ui_UsdStageRenderSettingsRollup.h"

#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <iInstanceMgr.h>
#include <iparamb2.h>

using namespace MaxSDK;

UsdStageRenderSettingsRollup::UsdStageRenderSettingsRollup(
    ReferenceMaker& owner,
    IParamBlock2&   paramBlock)
    : ui(new Ui::UsdStageRenderSettingsRollup)
{
    SetParamBlock((ReferenceMaker*)&owner, (IParamBlock2*)&paramBlock);

    ui->setupUi(this);
    modelObj = static_cast<USDStageObject*>(&owner);
}

UsdStageRenderSettingsRollup::~UsdStageRenderSettingsRollup() { }

void UsdStageRenderSettingsRollup::SetParamBlock(
    ReferenceMaker*     owner,
    IParamBlock2* const paramBlock)
{
    this->paramBlock = paramBlock;
    modelObj = static_cast<USDStageObject*>(owner);
}

void UsdStageRenderSettingsRollup::UpdateUI(const TimeValue t) { }

void UsdStageRenderSettingsRollup::UpdateParameterUI(
    const TimeValue t,
    const ParamID   paramId,
    const int /*tabIndex*/)
{
}

void UsdStageRenderSettingsRollup::on_AssignMtlToNodeButton_clicked() const
{
    // First figure out all the nodes that reference this USD stage (could have many in case
    // of instanced stages)
    auto getNodeFromObject = [](BaseObject* obj) {
        ULONG handle = 0;
        obj->NotifyDependents(FOREVER, (PartID)&handle, REFMSG_GET_NODE_HANDLE);
        INode* node = GetCOREInterface()->GetINodeByHandle(handle);
        return node;
    };
    INodeTab nodes;

    const auto firstNode = getNodeFromObject(modelObj);
    if (!firstNode) {
        return;
    }

    IInstanceMgr::GetInstanceMgr()->GetInstances(*firstNode, nodes);

    // Now apply the material on the node that is currently selected. We assume that only
    // one will be selected, as UI is accessible at the time when the button is pressed.
    for (int i = 0; i < nodes.Count(); ++i) {
        if (nodes[i] && nodes[i]->Selected()) {
            const auto usdPreviewSurfaceMtls = modelObj->GetUsdPreviewSurfaceMaterials(true);
            nodes[i]->SetMtl(usdPreviewSurfaceMtls);
            // If we have UI accessible, only expect a single selected node.
            break;
        }
    }
}
