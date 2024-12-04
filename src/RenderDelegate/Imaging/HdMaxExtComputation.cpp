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
#include "HdMaxExtComputation.h"

PXR_NAMESPACE_OPEN_SCOPE

HdMaxExtComputation::HdMaxExtComputation(pxr::SdfPath const& id)
    : HdExtComputation(id)
{
}

void HdMaxExtComputation::Sync(
    pxr::HdSceneDelegate* sceneDelegate,
    pxr::HdRenderParam*   renderParam,
    pxr::HdDirtyBits*     dirtyBits)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    HdExtComputation::_Sync(sceneDelegate, renderParam, dirtyBits);

    TF_DEBUG(pxr::HD_EXT_COMPUTATION_UPDATED)
        .Msg(
            "HdMaxExtComputation::Sync for %s (dirty bits = 0x%x)\n",
            GetId().GetText(),
            *dirtyBits);

    if (!(*dirtyBits & DirtySceneInput)) {
        // No scene inputs to sync. All other computation dirty bits (barring
        // DirtyCompInput) are sync'd in HdExtComputation::_Sync.
        return;
    }

    // Force pre-compute of "joint world inverse bind" transforms to work around USD concurrency
    // issue, see https://github.com/PixarAnimationStudios/USD/issues/1742

    // The "joint world inverse bind" transforms are not time dependant, so we only need to
    // force-compute them once.
    if (!_computeJointWorldInverseBindTransforms) {
        return;
    }

    for (pxr::TfToken const& inputName : GetSceneInputNames()) {
        sceneDelegate->GetExtComputationInput(GetId(), inputName);
    }

    _computeJointWorldInverseBindTransforms = false;
}

PXR_NAMESPACE_CLOSE_SCOPE