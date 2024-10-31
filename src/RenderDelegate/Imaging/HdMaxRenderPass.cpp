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
#include "HdMaxRenderPass.h"

#include "HdMaxRenderDelegate.h"

#include <pxr/base/trace/trace.h>

PXR_NAMESPACE_OPEN_SCOPE

HdMaxRenderPass::HdMaxRenderPass(HdRenderIndex* index, HdRprimCollection const& collection)
    : HdRenderPass(index, collection)
{
}

void HdMaxRenderPass::_Execute(
    HdRenderPassStateSharedPtr const& renderPassState,
    TfTokenVector const&              renderTags)
{
    TRACE_FUNCTION();

    bool authoredTagsChanged = false;
    auto ver = GetRenderIndex()->GetChangeTracker().GetRenderTagVersion();
    if (ver != authoredTagsVer) {
        authoredTagsChanged = true;
        authoredTagsVer = ver;
    }

    // Active render tags have changed, flag the render data for display accordingly.
    if (prevRenderTags != renderTags || authoredTagsChanged) {
        HdMaxRenderDelegate* renderDelegate
            = static_cast<HdMaxRenderDelegate*>(GetRenderIndex()->GetRenderDelegate());
        for (auto& rd : renderDelegate->GetAllRenderData()) {
            const auto&      id = rd.rPrimPath;
            HdSceneDelegate* sceneDelegate = GetRenderIndex()->GetSceneDelegateForRprim(id);
            auto             renderTag = sceneDelegate->GetRenderTag(id);
            bool             renderTagActive
                = std::find(renderTags.begin(), renderTags.end(), renderTag) != renderTags.end();

            // Need to force a rsync in this case - otherwise in some scenarios the mesh sync() is
            // not called and we can miss some geometry.
            if (rd.renderTagActive != renderTagActive) {
                GetRenderIndex()->GetChangeTracker().MarkRprimDirty(id);
                rd.renderTagActive = renderTagActive;
            }
        }
        prevRenderTags = renderTags;
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
