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
#include "HdMaxRenderTask.h"

#include <pxr/imaging/hd/renderDelegate.h>

PXR_NAMESPACE_OPEN_SCOPE

void HdMaxRenderTask::Sync(
    pxr::HdSceneDelegate* delegate,
    pxr::HdTaskContext*   ctx,
    pxr::HdDirtyBits*     dirtyBits)
{
    HdDirtyBits bits = *dirtyBits;

    if (bits & HdChangeTracker::DirtyCollection) {
        VtValue val = delegate->Get(GetId(), HdTokens->collection);

        HdRprimCollection collection = val.Get<HdRprimCollection>();

        // Check for cases where the collection is empty (i.e. default
        // constructed).  To do this, the code looks at the root paths,
        // if it is empty, the collection doesn't refer to any prims at
        // all.
        if (collection.GetName().IsEmpty()) {
            pass.reset();
        } else {
            if (!pass) {
                HdRenderIndex&    index = delegate->GetRenderIndex();
                HdRenderDelegate* renderDelegate = index.GetRenderDelegate();
                pass = renderDelegate->CreateRenderPass(&index, collection);
            } else {
                pass->SetRprimCollection(collection);
            }
        }
    }

    if ((*dirtyBits) & HdChangeTracker::DirtyRenderTags) {
        renderTags = _GetTaskRenderTags(delegate);
    }

    if (pass) {
        pass->Sync();
    }
}

void HdMaxRenderTask::Prepare(pxr::HdTaskContext* ctx, pxr::HdRenderIndex* renderIndex)
{
    if (!renderPassState) {
        HdRenderDelegate* renderDelegate = renderIndex->GetRenderDelegate();
        renderPassState = renderDelegate->CreateRenderPassState();
    }
}

void HdMaxRenderTask::Execute(pxr::HdTaskContext* ctx)
{
    if (!TF_VERIFY(renderPassState)) {
        return;
    }
    if (pass) {
        pass->Execute(renderPassState, GetRenderTags());
    }
}

const TfTokenVector& HdMaxRenderTask::GetRenderTags() const { return renderTags; }

PXR_NAMESPACE_CLOSE_SCOPE
